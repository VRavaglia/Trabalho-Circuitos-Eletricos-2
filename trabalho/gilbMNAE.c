/*
Programa de demonstracao de analise nodal modificada
Por Antonio Carlos M. de Queiroz acmq@coe.ufrj.br
Versao 1.0 - 6/9/2000
Versao 1.0a - 8/9/2000 Ignora comentarios
Versao 1.0b - 15/9/2000 Aumentado Yn, retirado Js
Versao 1.0c - 19/2/2001 Mais comentarios
Versao 1.0d - 16/2/2003 Tratamento correto de nomes em minusculas
Versao 1.0e - 21/8/2008 Estampas sempre somam. Ignora a primeira linha
Versao 1.0f - 21/6/2009 Corrigidos limites de alocacao de Yn
Versao 1.0g - 15/10/2009 Le as linhas inteiras
Versao 1.0h - 18/6/2011 Estampas correspondendo a modelos
Versao 1.0i - 03/11/2013 Correcoes em *p e saida com sistema singular.
Versao 1.0j - 26/11/2015 Evita operacoes com zero.
Versao 1.0k - 15/8/2018 Parametros de entrada. Mais nos e elementos.
*/

/*
Elementos aceitos e linhas do netlist:

Resistor:  R<nome> <no+> <no-> <resistencia>
VCCS:      G<nome> <io+> <io-> <vi+> <vi-> <transcondutancia>
VCVC:      E<nome> <vo+> <vo-> <vi+> <vi-> <ganho de tensao>
CCCS:      F<nome> <io+> <io-> <ii+> <ii-> <ganho de corrente>
CCVS:      H<nome> <vo+> <vo-> <ii+> <ii-> <transresistencia>
Fonte I:   I<nome> <io+> <io-> <corrente>
Fonte V:   V<nome> <vo+> <vo-> <tensao>
Amp. op.:  O<nome> <vo1> <vo2> <vi1> <vi2>
Capacitor: C<nome> <no+> <no-> <capacitancia> <tensao-inicial>
Indutor:   L<nome> <no+> <no-> <indutancia> <corrente-inicial>
Fonte VS.:  SV<nome> <vo+> <vo-> <Vmax> <frequencia> <fase-em-radianos> <offset>
Fonte IS.:  SI<nome> <io+> <io-> <Imax> <frequencia> <fase-em-radianos> <offset>

As fontes F e H tem o ramo de entrada em curto
O amplificador operacional ideal tem a saida suspensa
Os nos podem ser nomes
*/

#define versao "1.0k - 15/08/2018"
#include <stdio.h>

/*#include <conio.h>*/
#include <ncurses.h>

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#define MAX_LINHA 80
#define MAX_NOME 11
#define MAX_ELEM 500
#define MAX_NOS 100
#define TOLG 1e-15
#define STDDETAT 0.1
/*#define NUMINTER 20*/
#define PI 3.14159265359
#define MAXNR 50					/*maximo de interacoes permitidas no 
										newton-raphson*/

typedef enum boolean {falso=0, verdadeiro} boolean;

typedef struct elemento { /* Elemento do netlist */
	char nome[MAX_NOME],fType;		/*fType usado apenas por fontes*/
	double par1, par2, par3, par4, par5, par6, par7, par8, par9; /*par9 usado para viabilizar o numero de ciclos*/
	/*parametros na ordem em que aparecem na netlist*/
	int a,b,c,d,x,y;			/*nos (x,y sao para controle)*/
	struct elemento *auxComp;/*auxComp utilizado apenas por FF para 
										apontar ao bloco
							de reset*/
} elemento;

typedef struct tVarCell{
	int elNE;					/*guarda o numero do elemente variante no tempo*/
	struct tVarCell *next;
} tVarCell;

typedef struct dDCell{
	int elNE;					/*guarda o numero da fonte que varia no tempo (nao
										senoidal*/
	struct dDCell *next;
	boolean triggered,renovar;
} dDCell;

elemento netlist[MAX_ELEM+1]; /* Netlist */

int
  ne, /* Elementos */
  nv, /* Variaveis */
  nn, /* Nos */
  i,j,k;

boolean needBE=falso;

char
/* Foram colocados limites nos formatos de leitura para alguma protecao
   contra excesso de caracteres nestas variaveis */
  nomearquivo[MAX_LINHA+1],
  tipo,
  na[MAX_NOME],nb[MAX_NOME],nc[MAX_NOME],nd[MAX_NOME],
  lista[MAX_NOS+1][MAX_NOME+2], /* Tem que caber jx antes do nome */
  txt[MAX_LINHA+1],
  *p;

FILE *arquivo,*tabela;

double
	deltaT=STDDETAT,
	g,
	Yn[MAX_NOS+1][MAX_NOS+2];

/* Resolucao de sistema de equacoes lineares.
   Metodo de Gauss-Jordan com condensacao pivotal */
int resolversistema(void)
{
  int i,j,l, a;
  double t, p;

  for (i=1; i<=nv; i++) {
    t=0.0;
    a=i;
    for (l=i; l<=nv; l++) {
      if (fabs(Yn[l][i])>fabs(t)) {
	a=l;
	t=Yn[l][i];
      }
    }
    if (i!=a) {
      for (l=1; l<=nv+1; l++) {
	p=Yn[i][l];
	Yn[i][l]=Yn[a][l];
	Yn[a][l]=p;
      }
    }
    if (fabs(t)<TOLG) {
      printf("Sistema singular\n");
      return 1;
    }
    for (j=nv+1; j>i; j--) {  /* Basta j>i em vez de j>0 */
      Yn[i][j]/= t;
      p=Yn[i][j];
      if (p!=0)  /* Evita operacoes com zero */
        for (l=1; l<=nv; l++) {  
	  if (l!=i)
	    Yn[l][j]-=Yn[l][i]*p;
        }
    }
  }
  return 0;
}

/* Rotina que conta os nos e atribui numeros a eles */
int numero(char *nome)
{
  int i,achou;

  i=0; achou=0;
  while (!achou && i<=nv)
    if (!(achou=!strcmp(nome,lista[i]))) i++;
  if (!achou) {
    if (nv==MAX_NOS) {
      printf("O programa so aceita ate %d nos\n",nv);
      exit(1);
    }
    nv++;
    strcpy(lista[nv],nome);
    return nv; /* novo no */
  }
  else {
    return i; /* no ja conhecido */
  }
}

int main(int argc, char* argv[])
{
	int iTemp=0;											/*guarda o numero de interacoes no tempo*/
	unsigned iNR=0,j;						/*guarda o numero de interacoes 
											newton raphson*/
	double time,lastTime=0.0,tFinal;
	tVarCell *first, *cur,*aux;
	dDCell *firstD, *curD, *auxD, *earlier;
	char string[MAX_NOME+1],nomeTabela[MAX_NOME+1];
	double *nrValues,*lastValues,*lastLastValues;/*guarda os valores 
																anteriores/ultima
												interacao para o uso do metodo a analise
												de newton raphson respectivamente*/
	boolean operationPoint=falso,needNR=falso;

	first=(tVarCell *) NULL;
	firstD=(dDCell *) NULL;

  /*clrscr();*/system("cls");
  printf("Programa demonstrativo de analise nodal modificada\n");
  printf("Por Antonio Carlos M. de Queiroz - acmq@coe.ufrj.br\n");
  printf("Versao %s\n",versao);
 denovo:
  /* Leitura do netlist */
  ne=0; nv=0; strcpy(lista[0],"0");
  if (argc>1) {
    strcpy(nomearquivo,argv[1]);
    printf("Lendo arquivo %s\n",nomearquivo);
  }
  else {
    printf("Nome do arquivo com o netlist (ex: mna.net): ");
    scanf("%50s",nomearquivo);
  }
  arquivo=fopen(nomearquivo,"r");
  if (arquivo==0) {
    printf("Arquivo %s inexistente.\n",nomearquivo);
    argc=1;
    goto denovo;
  }

	strcpy(nomeTabela,nomearquivo);
	p=(char *) nomeTabela+strlen(nomeTabela);
	while (*(p) != '.') --p;
	*(++p)='t';*(++p)='a';*(++p)='b';*(++p)='\0';		/*escreve tab no final*/
	tabela=fopen(nomeTabela,"w");

	if (argc>2){
		if (argv[2][0] >= '0' && argv[2][0] <= '9')
			sscanf(argv[2],"%lg", &tFinal);
		else
			goto tempFinal;
	}
	else{
		tempFinal:
			printf("Insira o tempo final: ");
			scanf("%lg", &tFinal);
	}
	if (argc>3){
		if (argv[3][0] >= '0' && argv[3][0] <= '9')
			sscanf(argv[3],"%lg", &deltaT);
		else
			goto delT;
	}
	else{
		delT:
				printf("Insira o tamanho dos passos: ", &deltaT);
				scanf("%lg", &deltaT);
	}
	if (argc>4)
		if (toupper(argv[4][0]) == 'P' || toupper(argv[4][0]) == 'O')
			operationPoint=verdadeiro;		/*o padrao eh ponto de operacao*/

	time=-deltaT;

  printf("Lendo netlist:\n");
  fgets(txt,MAX_LINHA,arquivo);
  printf("Titulo: %s",txt);
  while (fgets(txt,MAX_LINHA,arquivo)) {
    ne++; /* Nao usa o netlist[0] */
    if (ne>MAX_ELEM) {
      printf("O programa so aceita ate %d elementos.\n",MAX_ELEM);
      exit(1);
    }
    txt[0]=toupper(txt[0]);
    tipo=txt[0];
    sscanf(txt,"%10s",netlist[ne].nome);
    p=txt+strlen(netlist[ne].nome); /* Inicio dos parametros */
    /* O que e lido depende do tipo */
    if (tipo=='R' || tipo=='I' || tipo=='V') {
		if (tipo=='I' || tipo=='V'){
			sscanf(p,"%10s%10s%10s%lg",na,nb,string,&netlist[ne].par1);
			netlist[ne].fType=toupper(string[0]);
			switch (netlist[ne].fType){
				case 'S':
					if (!needBE) needBE=verdadeiro;
					sscanf(p, "%10s%10s%10s%lg%lg%lg%lg%lg%lg%lg",na,nb,string,
						&netlist[ne].par1,&netlist[ne].par2,&netlist[ne].par3,
						&netlist[ne].par4,&netlist[ne].par5,&netlist[ne].par6,
						&netlist[ne].par7);
					printf("%s %s %s %s %g %g %g %g %g %g %g\n", netlist[ne].nome,na,nb,
					string,netlist[ne].par1,netlist[ne].par2,netlist[ne].par3,
					netlist[ne].par4,netlist[ne].par5,netlist[ne].par6,netlist[ne].par7);
					break;
				case 'P':
					if (!needBE) needBE=verdadeiro;
					curD = (dDCell *) malloc(sizeof(dDCell));
					(*curD).elNE=ne;
					(*curD).next= (dDCell *) NULL;
					(*curD).triggered=falso;
					(*curD).renovar=falso;
					if (!firstD) firstD=curD;
					else{
						auxD=firstD;
						while((*auxD).next) auxD=(*auxD).next;
						(*auxD).next=curD;
					}
					curD=firstD;
					sscanf(p, "%10s%10s%10s%lg%lg%lg%lg%lg%lg%lg%lg",na,nb,string,
							&netlist[ne].par1,&netlist[ne].par2,&netlist[ne].par3,
							&netlist[ne].par4,&netlist[ne].par5,&netlist[ne].par6,
							&netlist[ne].par7,&netlist[ne].par8);
					printf("%s %s %s %s %g %g %g %g %g %g %g %g\n", netlist[ne].nome,na,nb,
						string,netlist[ne].par1,netlist[ne].par2,netlist[ne].par3,
						netlist[ne].par4,netlist[ne].par5,netlist[ne].par6,
						netlist[ne].par7,netlist[ne].par8);
						netlist[ne].par9=netlist[ne].par8;
					break;
				default:
					sscanf(p,"%10s%10s%10s%lg",na,nb,string,&netlist[ne].par1);
					printf("%s %s %s %s %g\n",netlist[ne].nome,na,nb,string,netlist[ne].par1);
			}
		}
		else{
			sscanf(p,"%10s%10s%lg",na,nb,&netlist[ne].par1);
			printf("%s %s %g\n",netlist[ne].nome,na,nb,netlist[ne].par1);
		}
      netlist[ne].a=numero(na);
      netlist[ne].b=numero(nb);
    }

    else if (tipo=='G' || tipo=='E' || tipo=='F' || tipo=='H') {
      sscanf(p,"%10s%10s%10s%10s%lg",na,nb,nc,nd,&netlist[ne].par1);
      printf("%s %s %s %s %s %g\n",netlist[ne].nome,na,nb,nc,nd,netlist[ne].par1);
      netlist[ne].a=numero(na);
      netlist[ne].b=numero(nb);
      netlist[ne].c=numero(nc);
      netlist[ne].d=numero(nd);
    }
	else if (tipo=='C' || tipo=='L'){
		if (!needBE) needBE=verdadeiro;
		cur = (tVarCell *) malloc(sizeof(tVarCell));
		(*cur).elNE=ne;
		(*cur).next= (tVarCell *) NULL;
		if (!first) first=cur;
		else{
			aux=first;
			while((*aux).next) aux=(*aux).next;
			(*aux).next=cur;
		}
		sscanf(p,"%10s%10s%lg%lg",na,nb,&netlist[ne].par1,&netlist[ne].par2);
		printf("%s %s %s %g %g\n",netlist[ne].nome,na,nb,netlist[ne].par1,netlist[ne].par2);
		netlist[ne].a=numero(na);
		netlist[ne].b=numero(nb);
	}

	else if (tipo==')'||tipo=='('||tipo=='}'||tipo=='{'){/*portas*/
		sscanf(p,"%10s%10s%10s%lg%lg%lg%lg",na,nb,nc,&netlist[ne].par1,
				&netlist[ne].par2,&netlist[ne].par3,&netlist[ne].par4);/*par4 == A*/
		printf("%s %s %s %s %g %g %g %g\n",netlist[ne].nome,na,nb,nc,
				netlist[ne].par1,netlist[ne].par2,netlist[ne].par3,
				netlist[ne].par4);
		netlist[ne].a=numero(na);
		netlist[ne].b=numero(nb);
		netlist[ne].c=numero(nc);
	}
	else if (tipo=='%'){/*flip-flop*/
		char auxComp[MAX_NOME];
		sscanf(p,"%10s%10s%10s%10s%10s%lg%lg%lg",na,nb,nc,nd,
				auxComp,&netlist[ne].par1,&netlist[ne].par2,
				&netlist[ne].par3);
		printf("%s %s %s %s %s %g %g %g\n",netlist[ne].nome,na,nb,nc,nd,
				auxComp,netlist[ne].par1,netlist[ne].par2,
				netlist[ne].par3);
		netlist[ne].a=numero(na);
		netlist[ne].b=numero(nb);
		netlist[ne].c=numero(nc);
		netlist[ne].d=numero(nd);
		netlist[nv].auxComp= (elemento *) NULL;
		for(i=0; i<ne; i++){
			if (!strcmp(auxComp,netlist[i].nome))
				netlist[nv].auxComp= (elemento *) &netlist[i];
		}
	}
	else if (tipo=='!'){
		sscanf(p,"%10s%10s%lg%lg",na,nb,&netlist[ne].par1,&netlist[ne].par2);
		printf("%s %s %s %g %g\n",na,nb,&netlist[ne].par1,&netlist[ne].par2);
		netlist[ne].a=numero(na);
		netlist[ne].b=numero(nb);
	}
    else if (tipo=='O') {
      sscanf(p,"%10s%10s%10s%10s",na,nb,nc,nd);
      printf("%s %s %s %s %s\n",netlist[ne].nome,na,nb,nc,nd);
      netlist[ne].a=numero(na);
      netlist[ne].b=numero(nb);
      netlist[ne].c=numero(nc);
      netlist[ne].d=numero(nd);
    }
    else if (tipo=='*') { /* Comentario comeca com "*" */
      printf("Comentario: %s",txt);
      ne--;
    }
    else {
      printf("Elemento desconhecido: %s\n",txt);
      getch();
      exit(1);
    }
  }
  fclose(arquivo);
  /* Acrescenta variaveis de corrente acima dos nos, anotando no netlist */
  nn=nv;
  for (i=1; i<=ne; i++) {
    tipo=netlist[i].nome[0];
    if (tipo=='V' || tipo=='E' || tipo=='F' || tipo=='O' || tipo=='L') {
      nv++;
      if (nv>MAX_NOS) {
        printf("As correntes extra excederam o numero de variaveis permitido (%d)\n",MAX_NOS);
        exit(1);
      }
      strcpy(lista[nv],"j"); /* Tem espaco para mais dois caracteres */
      strcat(lista[nv],netlist[i].nome);
      netlist[i].x=nv;
    }
    else if (tipo=='H') {
      nv=nv+2;
      if (nv>MAX_NOS) {
        printf("As correntes extra excederam o numero de variaveis permitido (%d)\n",MAX_NOS);
        exit(1);
      }
      strcpy(lista[nv-1],"jx"); strcat(lista[nv-1],netlist[i].nome);
      netlist[i].x=nv-1;
      strcpy(lista[nv],"jy"); strcat(lista[nv],netlist[i].nome);
      netlist[i].y=nv;
    }
  }
	itsCur:
  getch();
  printf("Variaveis internas: \n");
  for (i=0; i<=nv; i++)
    printf("%d -> %s\n",i,lista[i]);
  getch();
  printf("Netlist interno final\n");
  for (i=1; i<=ne; i++) {
    tipo=netlist[i].nome[0];
	if (tipo=='R')
		printf("%s %d %d %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].par1);
	if (tipo=='I' || tipo=='V'){
		if (netlist[i].fType=='S'){
			strcpy(string,"SIN");
			printf("%s %d %d %s %g %g %g %g %g %g %g\n", netlist[i].nome,netlist[i].a,
			netlist[i].b,string,netlist[i].par1,netlist[i].par2,netlist[i].par3,
			netlist[i].par4,netlist[i].par5,netlist[i].par6, netlist[i].par7);
		}
		else if (netlist[i].fType=='P'){
			strcpy(string,"PULSE");
			printf("%s %d %d %s %g %g %g %g %g %g %g %g\n", netlist[i].nome,netlist[i].a,
			netlist[i].b,string,netlist[i].par1,netlist[i].par2,netlist[i].par3,
			netlist[i].par4,netlist[i].par5,netlist[i].par6, netlist[i].par7,
			netlist[i].par8);
		}
		else{
			strcpy(string,"DC");
			printf("%s %d %d %s %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,string,
			netlist[i].par1);
		}
	}
	else if (tipo=='C' || tipo=='L')
		printf("%s %d %d %g %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,
				netlist[i].par1,netlist[i].par2);
	else if (tipo==')'||tipo=='('||tipo=='{'||tipo=='}')
		printf("%s %d %d %g %g %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,
				netlist[i].par1,netlist[i].par2,netlist[i].par3);
    else if (tipo=='G' || tipo=='E' || tipo=='F' || tipo=='H') {
      printf("%s %d %d %d %d %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].c,netlist[i].d,netlist[i].par1);
    }
    else if (tipo=='O') {
      printf("%s %d %d %d %d\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].c,netlist[i].d);
    }
    if (tipo=='V' || tipo=='E' || tipo=='F' || tipo=='O' || tipo=='L')
      printf("Corrente jx: %d\n",netlist[i].x);
    else if (tipo=='H')
      printf("Correntes jx e jy: %d, %d\n",netlist[i].x,netlist[i].y);
  }
  getch();
  /* Monta o sistema nodal modificado */
  printf("O circuito tem %d nos, %d variaveis e %d elementos\n",nn,nv,ne);
  getch();

	nrValues=(double *) malloc(sizeof(double)*(nv));
	lastValues=(double *) malloc(sizeof(double)*(nv));
	lastLastValues=(double *) malloc(sizeof(double)*(nv));

	for (i=0; i<nv; i++)
		nrValues[i]=lastValues[i]=lastLastValues[i]=0.0;

	buildSys:
	time+=deltaT;
/*	{
		earlier = curD = (dDCell *) NULL;
*/
		curD=firstD;
		while(curD){
			if ((((netlist[(*curD).elNE].par3)+netlist[(*curD).elNE].par7*
			(netlist[(*curD).elNE].par8-netlist[(*curD).elNE].par9)) <= time) &&
				!((*curD).triggered))/*{
					time= (double) netlist[(*curD).elNE].par3;
					earlier=curD;
			}*/
				(*curD).triggered=verdadeiro;
			curD=(*curD).next;
		}
/*		if (earlier) (*earlier).triggered=verdadeiro;
	}
*/
	newtonRaphson:
	needNR=falso;

	if (++iNR>MAXNR){
		printf("Sistema não convergiu\n");
		goto endProg;
	}


  /* Zera sistema */
  for (i=0; i<=nv; i++) {
    for (j=0; j<=nv+1; j++)
      Yn[i][j]=0;
  }
	if (time-lastTime<deltaT){
		if (lastTime==0.0)
			time=lastTime;
		else
			time=(lastTime+deltaT);
	}
  /* Monta estampas */
	if (firstD) curD=firstD;
  for (i=1; i<=ne; i++) {

		tipo=netlist[i].nome[0];
    if (tipo=='R' || tipo=='C') {
		if (tipo=='R')
			g=1/netlist[i].par1;
		else{
			if (time==0.0){
				if (operationPoint)
					g=netlist[i].par1/1e10;
				else
					g=netlist[i].par1/1e-10;
			}
			else
				g=netlist[i].par1/deltaT;		/*verificar*/
		}
		Yn[netlist[i].a][netlist[i].a]+=g;
		Yn[netlist[i].b][netlist[i].b]+=g;
		Yn[netlist[i].a][netlist[i].b]-=g;
		Yn[netlist[i].b][netlist[i].a]-=g;
		if (tipo=='C'){
			g=-(g*netlist[i].par2);	/*verificar*/
	 		goto insertCur;
		}
	}
    else if (tipo=='G') {
      g=netlist[i].par1;
      Yn[netlist[i].a][netlist[i].c]+=g;
      Yn[netlist[i].b][netlist[i].d]+=g;
      Yn[netlist[i].a][netlist[i].d]-=g;
      Yn[netlist[i].b][netlist[i].c]-=g;
    }
	else if (tipo=='I') {
		switch(netlist[i].fType){
			case 'S':
				g=netlist[i].par1+netlist[i].par2*(exp(-netlist[ne].par5*
				(time-netlist[ne].par4)))*sin(2*PI*(netlist[i].par3)*(time-
				netlist[ne].par4)-((PI/180)*netlist[i].par6)); /*ver como incluir numCic*/
				break;
			case 'P':
				if (i != (*curD).elNE){
					printf("Um error interno ocorreu\n");
					exit(1);
				}
				g=0;
				if ((*curD).triggered){			/*revisar isso*/
					if ((netlist[i].par3+netlist[i].par4+((netlist[i].par8-netlist[i].par9)*netlist[i].par7)) > time)
						g=netlist[i].par1+(((netlist[i].par1-netlist[i].par2)/
						netlist[i].par4)*(time-(netlist[i].par3)+((netlist[i].par8-netlist[i].par9)*netlist[i].par7)));
					else if ((netlist[i].par3+netlist[i].par4+netlist[i].par6
					+((netlist[i].par8-netlist[i].par9)*netlist[i].par7))>time)			/*talvez >=*/
						g=netlist[i].par2;
					else if ((netlist[i].par3+netlist[i].par4+netlist[i].par6+
								netlist[i].par5+((netlist[i].par8-netlist[i].par9)*netlist[i].par7)) > time){ /*talvez >=*/
						g=netlist[i].par2-(((netlist[i].par2-netlist[i].par1)/
						netlist[i].par5)*(time-(netlist[i].par3+((netlist[i].par8-netlist[i].par9)*netlist[i].par7)+
						netlist[i].par4+netlist[i].par6)));
						if ((time+deltaT)>=(netlist[i].par3+netlist[i].par4+netlist[i].par6+
							netlist[i].par5+((netlist[i].par8-netlist[i].par9)*netlist[i].par7))) goto renew;
					}
					else{
						if (netlist[i].par3+(netlist[i].par7*(netlist[i].par8-netlist[i].par9)) > time
						|| netlist[i].par9==0){
							g=netlist[i].par1;					/*desligado fica na amplitude inicial*/
						}
						renew:
							(*curD).renovar=verdadeiro;
					}
					curD=(*curD).next;
				}
				else g=netlist[i].par1;							/*desligado fica na amplitude inicial*/
				curD=(*curD).next;			/*sera*/
				break;
			default:
				g=netlist[i].par1;
		}
		insertCur:
			Yn[netlist[i].a][nv+1]-=g;
			Yn[netlist[i].b][nv+1]+=g;
	}
	else if (tipo=='V' || tipo=='L') {
		if (tipo=='V'){
			switch(netlist[i].fType){
				case 'S':
					g=netlist[i].par1+netlist[i].par2*(exp(-netlist[ne].par5*
					(time-netlist[ne].par4)))*sin(2*PI*(netlist[i].par3)*(time-
					netlist[ne].par4)-((PI/180)*netlist[i].par6)); /*ver como incluir numCic*/
					break;
				case 'P':
					if (i != (*curD).elNE){
						printf("Um error interno ocorreu\n");
						exit(1);
					}
					g=0;
					if ((*curD).triggered){			/*revisar isso*/
						if ((netlist[i].par3+netlist[i].par4+((netlist[i].par8-netlist[i].par9)*netlist[i].par7)) > time)
							g=netlist[i].par1+(((netlist[i].par1-netlist[i].par2)/
							netlist[i].par4)*(time-((netlist[i].par3)+((netlist[i].par8-netlist[i].par9)*netlist[i].par7))));
						else if ((netlist[i].par3+netlist[i].par4+netlist[i].par6
						+((netlist[i].par8-netlist[i].par9)*netlist[i].par7))>time)			/*talvez >=*/
							g=netlist[i].par2;
						else if ((netlist[i].par3+netlist[i].par4+netlist[i].par6+
									netlist[i].par5+((netlist[i].par8-netlist[i].par9)*netlist[i].par7)) > time){ /*talvez >=*/
							g=netlist[i].par2-(((netlist[i].par2-netlist[i].par1)/
							netlist[i].par5)*(time-(netlist[i].par3+((netlist[i].par8-netlist[i].par9)*netlist[i].par7)
							+netlist[i].par4+netlist[i].par6)));
							if ((time+deltaT)>=(netlist[i].par3+netlist[i].par4+netlist[i].par6+
								netlist[i].par5+((netlist[i].par8-netlist[i].par9)*netlist[i].par7))) goto renew2;
						}
						else{
							if (netlist[i].par3+(netlist[i].par7*(netlist[i].par8-netlist[i].par9))
							> time || netlist[i].par9==0){
								g=netlist[i].par1;					/*desligado fica na amplitude inicial*/
							}
							renew2:
								(*curD).renovar=verdadeiro;
						}
					}
					else g=netlist[i].par1;							/*desligado fica na amplitude inicial*/
					curD=(*curD).next;
					break;
				default:
					g=netlist[i].par1;
			}
			insertVolt:
				Yn[netlist[i].x][nv+1]-=g;
		}
		else{
			if (time==0.0){
				if (operationPoint)
					g=netlist[i].par1/1e10;
				else
					g=netlist[i].par1/1e-10;
			}
			else
				g=netlist[i].par1/deltaT;
			Yn[netlist[i].x][netlist[i].x]+=g;
			Yn[netlist[i].x][nv+1]+=g*netlist[i].par2;	/*verificar*/
		}
      Yn[netlist[i].a][netlist[i].x]+=1;
      Yn[netlist[i].b][netlist[i].x]-=1;
      Yn[netlist[i].x][netlist[i].a]-=1;
      Yn[netlist[i].x][netlist[i].b]+=1;
   }
	else if (tipo=='('||tipo==')'||tipo=='{'||tipo=='}'){/*portas*/
		double vX,a1,a2,vOut,
				voltage1,voltage2;			/*organinizados na ordem que devem
													ser verificados (e1 > e2)*/
		switch (tipo) {
			case (')'):
			case ('('):
				voltage1=lastValues[netlist[i].a];
				voltage2=lastValues[netlist[i].b];
/*				voltage1=nrValues[netlist[i].a];
				voltage2=nrValues[netlist[i].b];*/
				break;
			case ('{'):
			case ('}'):
				voltage1=lastValues[netlist[i].b];
				voltage2=lastValues[netlist[i].a];
/*		voltage2=nrValues[netlist[i].a];
				voltage1=nrValues[netlist[i].b];*/
				break;
		}
		switch (tipo){
			case ('('):/*estes estao dando erro de convergencia*/
			case ('{'):
				if (voltage1>voltage2){
					vX=(netlist[i].par1/2)-(netlist[i].par4*(nrValues[
					netlist[i].b]-(netlist[i].par1/2)));
					a1=0.0;							/*talvez mude*/
					a2=-netlist[i].par4;
				}
				else{
					vX=(netlist[i].par1/2)-(netlist[i].par4*(nrValues[
					netlist[i].a]-(netlist[i].par1/2)));/*c==a*/
					a1=-netlist[i].par4;
					a2=0.0;							/*talvez mude*/
				}
				if (vX>netlist[i].par1){
					vOut=netlist[i].par1;
					a2=a1=0.0;
				}
				else if (vX < 0.0){
					vOut=0.0;
					a1=a2=0.0;
				}
				else
					vOut=(netlist[i].par1/2)*(1+netlist[i].par4);
				break;			/*por enquanto*/
			case (')'):
			case ('}'):
				if (voltage1>voltage2){
					vX=(netlist[i].par1/2)+(netlist[i].par4*(nrValues[
					netlist[i].b]-(netlist[i].par1/2)));
					a1=0.0;							/*talvez mude*/
					a2=netlist[i].par4;
				}
				else{
					vX=(netlist[i].par1/2)+(netlist[i].par4*(nrValues[
					netlist[i].a]-(netlist[i].par1/2)));/*c==a*/
					a1=netlist[i].par4;
					a2=0.0;							/*talvez mude*/
				}
				if (vX>netlist[i].par1){
					vOut=netlist[i].par1;
					a2=a1=0.0;
				}
				else if (vX < 0.0){
					vOut=0.0;
					a1=a2=0.0;
				}
				else
					vOut=(netlist[i].par1/2)*(1-netlist[i].par4);
				break;			/*por enquanto*/
		}
		g=1/netlist[i].par2;
		Yn[netlist[i].c][netlist[i].c]+=g;
		Yn[netlist[i].c][nv+1]+=g*vOut;
		Yn[netlist[i].c][netlist[i].a]-=g*a1;			/*+?*/
		Yn[netlist[i].c][netlist[i].b]-=g*a2;			/*+?*/
		/*acrescentar os capacitores de entrada*/
		if (time==0.0){
			if (operationPoint)
				g=netlist[i].par3/1e10;
			else
				g=netlist[i].par3/1e-10;
		}
		else
			g=netlist[i].par3/deltaT;
		Yn[netlist[i].a][netlist[i].a]+=g;
		Yn[netlist[i].b][netlist[i].b]+=g;
		g=(g*lastValues[netlist[i].a]);	/*verificar*/
		Yn[netlist[i].a][nv+1]+=g;
		g=(g*lastValues[netlist[i].b]);	/*verificar*/
		Yn[netlist[i].b][nv+1]+=g;
	}
	else if (tipo=='%'){
		double Qplus, Qminus;
		if (netlist[i].auxComp){
			if (lastValues[netlist[i].auxComp->b] >=
				(netlist[i].auxComp->par1/2)){
					Qplus=0.0;
					Qminus=netlist[i].par1;
			}
			else if (lastValues[netlist[i].auxComp->a] >=
						(netlist[i].auxComp->par1/2)){
					Qminus=0.0;
					Qplus=netlist[i].par1;
			}
			else goto flipFlop;
		}
		else{
			flipFlop:
			if (lastValues[netlist[i].d] >= (netlist[i].par1/2)){
				if (lastLastValues[netlist[i].d] <= (netlist[i].par1/2)){
					if (lastValues[netlist[i].a]>0){
						Qplus=0.0;
						Qminus=netlist[i].par1;
					}
					else{
						Qplus=netlist[i].par1;
						Qminus=0.0;
					}
				}
			}
			else if (time<deltaT){
				Qplus=0.0;
				Qminus=netlist[i].par1;
			}
		}
		g=1/netlist[i].par2;
		Yn[netlist[i].a][netlist[i].a]+=g;
		Yn[netlist[i].b][netlist[i].b]+=g;
		Yn[netlist[i].a][nv+1]+=g*Qplus;
		Yn[netlist[i].b][nv+1]+=g*Qminus;
		/*acrescentar os capacitores para terra*/
		if (time==0.0){
			if (operationPoint)
				g=netlist[i].par3/1e10;
			else
				g=netlist[i].par3/1e-10;
		}
		else
			g=netlist[i].par3/deltaT;
		Yn[netlist[i].c][netlist[i].c]+=g;
		g=(g*lastValues[netlist[i].c]);	/*verificar*/
		Yn[netlist[i].c][nv+1]+=g;
	}

    else if (tipo=='E') {
      g=netlist[i].par1;
      Yn[netlist[i].a][netlist[i].x]+=1;
      Yn[netlist[i].b][netlist[i].x]-=1;
      Yn[netlist[i].x][netlist[i].a]-=1;
      Yn[netlist[i].x][netlist[i].b]+=1;
      Yn[netlist[i].x][netlist[i].c]+=g;
      Yn[netlist[i].x][netlist[i].d]-=g;
    }
    else if (tipo=='F') {
      g=netlist[i].par1;
      Yn[netlist[i].a][netlist[i].x]+=g;
      Yn[netlist[i].b][netlist[i].x]-=g;
      Yn[netlist[i].c][netlist[i].x]+=1;
      Yn[netlist[i].d][netlist[i].x]-=1;
      Yn[netlist[i].x][netlist[i].c]-=1;
      Yn[netlist[i].x][netlist[i].d]+=1;
    }
    else if (tipo=='H') {
      g=netlist[i].par1;
      Yn[netlist[i].a][netlist[i].y]+=1;
      Yn[netlist[i].b][netlist[i].y]-=1;
      Yn[netlist[i].c][netlist[i].x]+=1;
      Yn[netlist[i].d][netlist[i].x]-=1;
      Yn[netlist[i].y][netlist[i].a]-=1;
      Yn[netlist[i].y][netlist[i].b]+=1;
      Yn[netlist[i].x][netlist[i].c]-=1;
      Yn[netlist[i].x][netlist[i].d]+=1;
      Yn[netlist[i].y][netlist[i].x]+=g;
    }
    else if (tipo=='O') {
      Yn[netlist[i].a][netlist[i].x]+=1;
      Yn[netlist[i].b][netlist[i].x]-=1;
      Yn[netlist[i].x][netlist[i].c]+=1;
      Yn[netlist[i].x][netlist[i].d]-=1;
    }
#ifdef DEBUG
    /* Opcional: Mostra o sistema apos a montagem da estampa */
    printf("Sistema apos a estampa de %s\n",netlist[i].nome);
    for (k=1; k<=nv; k++) {
      for (j=1; j<=nv+1; j++)
        if (Yn[k][j]!=0) printf("%+3.1f ",Yn[k][j]);
        else printf(" ... ");
      printf("\n");
    }
    getch();
#endif
  }
  /* Resolve o sistema */
  if (resolversistema()) {
    getch();
    exit(1);
  }

/*	printf("(#%g)",time);*/
	for (j=1; j<nv; j++)			/*newton raphson*/
	{
/*		if (j==1)
			printf("\n\n%lg",Yn[j][nv+1]);
		else
			printf(" %lg",Yn[j][nv+1]);*/
		if (nrValues[j]!=Yn[j][nv+1]){
			needNR=verdadeiro;
			nrValues[j]=Yn[j][nv+1];
		}
	}
/*	printf("\n");*/

	if (needNR)
		goto newtonRaphson;
	iNR=0;

#ifdef DEBUG
  /* Opcional: Mostra o sistema resolvido */
  printf("Sistema resolvido:\n");
  for (i=1; i<=nv; i++) {
      for (j=1; j<=nv+1; j++)
        if (Yn[i][j]!=0) printf("%+3.1f ",Yn[i][j]);
        else printf(" ... ");
      printf("\n");
    }
  getch();
#endif
  /* Salvar solucao */
	if (time==0.0){
		for (i=1; i<=nv; i++) {
			if(i==1)
				fprintf(tabela, "%s", "t ");
			if(i!=nv)
				fprintf(tabela, "%s ", lista[i]);
			else
				fprintf(tabela, "%s\n", lista[i]);
		}
  }
	for (i=1; i<=nv; i++){
		if (i==1) fprintf(tabela,"%g ",time);
		if (i!=nv)
			fprintf(tabela,"%g ",Yn[i][nv+1]);
		else{
			if (time <= tFinal)
				fprintf(tabela,"%g\n",Yn[i][nv+1]);
			else
				fprintf(tabela,"%g",Yn[i][nv+1]);
		}
	}
  getch();
	if (needBE)
		if (time<=tFinal){
			cur=first;
			while (cur){
				if (netlist[(*cur).elNE].nome[0]=='C')
					netlist[(*cur).elNE].par2=Yn[netlist[(*cur).elNE].a][nv+1]-Yn[netlist[(*cur).elNE].b][nv+1];
				else
					netlist[(*cur).elNE].par2=Yn[netlist[(*cur).elNE].x][nv+1];
				cur=(*cur).next;
			}
			curD=firstD;
			while (curD){
				if ((*curD).renovar){
					(*curD).triggered=falso;
					(*curD).renovar=falso;
					netlist[(*curD).elNE].par9--;
				}
				curD=(*curD).next;
			}
			if (time>=deltaT)
				for (i=0; i<nv; i++)
					lastLastValues[i]=lastValues[i];
			for (i=0; i<nv; i++)
				lastValues[i]=nrValues[i];

			lastTime=time;
			goto buildSys;
		}
		else
		{
			endProg:{
				fclose(tabela);
				cur=first;
				while(cur){
					aux=(*cur).next;
					free(cur);
					cur=aux;
				}
				curD=firstD;
				while(curD){
					auxD=(*curD).next;
					free(curD);
					curD=auxD;
				}
			}
		}
  return 0;
}
