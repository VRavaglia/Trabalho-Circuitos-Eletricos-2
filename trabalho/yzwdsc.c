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
#include <time.h>

#define MAX_LINHA 80
#define MAX_NOME 11
#define MAX_ELEM 500
#define MAX_NOS 100
#define TOLG 1e-25
#define STDDETAT 0.1
/*#define NUMINTER 20*/
#define PI 3.14159265359
#define MAXNR 5					/*maximo de ciclos permitidos no 
										newton-raphson*/

typedef enum boolean {falso=0, verdadeiro} boolean;

typedef struct elemento { /* Elemento do netlist */
	char nome[MAX_NOME],fType;		/*fType usado apenas por fontes*/
	double par1, par2, par3, par4, par5, par6, par7;
	unsigned par8, par9; /*numero de ciclos*/
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
	double internTimer;
	struct dDCell *next;
	boolean triggered,renovar;
} dDCell;

typedef struct dSCell{	/*guarda o numero da fonte senoidal*/
	int elNE;
	double curPer;
	struct dSCell *next;
} dSCell;

typedef struct ffCell{	/*usado apenas pelo flip-flop*/
	int elNE;
	boolean high;
	struct ffCell *next;
}ffCell;

typedef struct mCell{	/*guarda quando o monoestavel deve ser desligado*/
	int elNE;
	double turnDown;
	struct mCell *next;
	boolean up;
} mCell;

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
	deltaT=0.0,
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
	unsigned iNR=0,maxNR=0,cicNR,j;	/*guarda o numero de interacoes 
															newton raphson*/
	double curTime,lastTime=0.0,tFinal=0.0,curTimePrecision;
	tVarCell *first, *cur, *aux;
	dDCell *firstD, *curD, *auxD, *earlier;
	dSCell *firstS, *curS, *auxS;
	mCell *firstM, *curM, *auxM;
	ffCell *firstFF, *curFF, *auxFF;
	char string[MAX_NOME+1],nomeTabela[MAX_NOME+1],icOpo[MAX_NOME+1];
	double *nrValues,*lastNRValues,*lastValues,*lastLastValues;
												/*guarda os valores 
												anteriores/ultima
												interacao para o uso do metodo a analise
												de newton raphson respectivamente*/
	boolean operationPoint=verdadeiro,needNR=falso;

	first=(tVarCell *) NULL;
	firstD=(dDCell *) NULL;
	firstS=(dSCell *) NULL;
	firstM=(mCell *) NULL;
	firstFF=(ffCell *) NULL;

  /*clrscr();*/system("cls");
  printf("Programa de analise nodal modificada de elementos lineares\n");
  printf("e nao lineares usando os metodos \"backward\" de Euler e de\n");
  printf("Newton-Raphson.\n");
  printf("Grupo: Mateus Gilbert, Patrick Franco e Victor Raposo\n");
  printf("Periodo: 2018.2\n");
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
					sscanf(p, "%10s%10s%10s%lg%lg%lg%lg%lg%lg%u",na,nb,string,
						&netlist[ne].par1,&netlist[ne].par2,&netlist[ne].par3,
						&netlist[ne].par4,&netlist[ne].par5,&netlist[ne].par6,
						&netlist[ne].par8);
					printf("%s %s %s %s %g %g %g %g %g %g %u\n", netlist[ne].nome,na,nb,
					string,netlist[ne].par1,netlist[ne].par2,netlist[ne].par3,
					netlist[ne].par4,netlist[ne].par5,netlist[ne].par6,netlist[ne].par8);
					netlist[ne].par9=netlist[ne].par8;
					if (!needBE) needBE=verdadeiro;
					curS = (dSCell *) malloc(sizeof(dSCell));
					(*curS).elNE=ne;
					(*curS).next=(dSCell *) NULL;
					(*curS).curPer=netlist[ne].par4+(1/netlist[ne].par3);
					if (!firstS) firstS=curS;
					else{
						auxS=firstS;
						while((*auxS).next) auxS=(*auxS).next;
						(*auxS).next=curS;
					}
					curS=firstS;
					break;
				case 'P':
					if (!needBE) needBE=verdadeiro;
					curD = (dDCell *) malloc(sizeof(dDCell));
					(*curD).elNE=ne;
					(*curD).next=(dDCell *) NULL;
					(*curD).internTimer=0.0;
					(*curD).triggered=falso;
					(*curD).renovar=falso;
					if (!firstD) firstD=curD;
					else{
						auxD=firstD;
						while((*auxD).next) auxD=(*auxD).next;
						(*auxD).next=curD;
					}
					curD=firstD;
					sscanf(p, "%10s%10s%10s%lg%lg%lg%lg%lg%lg%lg%u",na,nb,string,
							&netlist[ne].par1,&netlist[ne].par2,&netlist[ne].par3,
							&netlist[ne].par4,&netlist[ne].par5,&netlist[ne].par6,
							&netlist[ne].par7,&netlist[ne].par8);
					printf("%s %s %s %s %g %g %g %g %g %g %g %u\n", netlist[ne].nome,na,nb,
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
		curFF = (ffCell *) malloc(sizeof(ffCell));
		(*curFF).elNE=ne;
		(*curFF).next=(ffCell *) NULL;
		(*curFF).high=falso;
		if (!firstFF) firstFF=curFF;
		else{
			auxFF=firstFF;
			while((*auxFF).next) auxFF=(*auxFF).next;
			(*auxFF).next=curFF;
		}
		curFF=firstFF;
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
	else if (tipo=='@'){/*monoestavel*/
		sscanf(p,"%10s%10s%10s%10s%lg%lg%lg%lg",na,nb,nc,nd,
				&netlist[ne].par1,&netlist[ne].par2,&netlist[ne].par3,
				&netlist[ne].par4);
		curM=(mCell *) malloc(sizeof(mCell));
		(*curM).elNE=ne;
		(*curM).next=(mCell *) NULL;
		(*curM).up=falso;
		if (!firstM) firstM=curM;
		else{
			auxM=firstM;
			while((*auxM).next) auxM=(*auxM).next;
			(*auxM).next=curM;
		}
		curM=firstM;
		printf("%s %s %s %s %g %g %g %g\n",netlist[ne].nome,na,nb,nc,nd,
				netlist[ne].par1,netlist[ne].par2,netlist[ne].par3,
				netlist[ne].par4);
		netlist[ne].a=numero(na);
		netlist[ne].b=numero(nb);
		netlist[ne].c=numero(nc);
		netlist[ne].d=numero(nd);
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
	else if (tipo=='.'){
		char analysisTipe[MAX_NOME];
		strcpy(icOpo,"\0");
		sscanf(p,"%lg%lg%10s%u%10s",&tFinal,&deltaT,analysisTipe,&maxNR,icOpo);
		if (strlen(icOpo))
			if (toupper(icOpo[1]=='I'))
				operationPoint=falso;
		printf("%s %g %g %s %u %s\n",netlist[ne].nome,tFinal,deltaT,analysisTipe,
												maxNR,icOpo);
		ne--;
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
	if (tFinal==0.0){
		printf("Insira o tempo final para analise: ");
		scanf("%lg",&tFinal);
		printf("Insira o tamanho dos passos: ");
		scanf("%lg",&deltaT);
		printf("Insira o numero de passos por ponto na tabela: ");
		scanf("%u",&maxNR);
		printf("Usar condicoes iniciais? [S]im ou [N]ao: ");
		scanf("%10s",icOpo);
		if (toupper(icOpo[0])=='S')
			operationPoint=falso;
	}
	curTime=-deltaT;
	curTimePrecision=deltaT/2;
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
			printf("%s %d %d %s %g %g %g %g %g %g %g %u\n", netlist[i].nome,netlist[i].a,
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
	lastNRValues=(double *) malloc(sizeof(double)*(nv));
	lastValues=(double *) malloc(sizeof(double)*(nv));
	lastLastValues=(double *) malloc(sizeof(double)*(nv));

	for (i=0; i<nv; i++)
		nrValues[i]=lastValues[i]=lastLastValues[i]=0.0;

	buildSys:
	curTime+=deltaT;

	curD=firstD;
	while(curD){
		if (((netlist[(*curD).elNE].par3) <= curTime) &&
			!((*curD).triggered)){
				(*curD).triggered=verdadeiro;
				(*curD).internTimer=deltaT+(curTime-netlist[(*curD).elNE].par3);
		}
		curD=(*curD).next;
	}

	newtonRaphson:
	needNR=falso;

	if (maxNR > 1){
		if (++iNR>maxNR){
			if (cicNR<=MAXNR){
				srand(time(NULL));
				for (i=0; i<nv; i++)
				if (lastNRValues[i]-nrValues[i] > 1e-7 ||
				nrValues[i]-lastNRValues[i] > 1e-7 ){
					nrValues[i]= (double) rand();
					if (nrValues[i] > 1e7){
						nrValues[i]/=1e7;
					}
				}
			}
			else{
				printf("Sistema não convergiu\n");
				goto endProg;
			}
		}
	}


  /* Zera sistema */
  for (i=0; i<=nv; i++) {
    for (j=0; j<=nv+1; j++)
      Yn[i][j]=0;
  }
	if (curTime-lastTime<deltaT){
		if (lastTime==0.0)
			curTime=lastTime;
		else
			curTime=(lastTime+deltaT);
	}
  /* Monta estampas */
	if (firstD) curD=firstD;
	if (firstS) curS=firstS;
	if (firstM) curM=firstM;
	if (firstFF) curFF=firstFF;

  for (i=1; i<=ne; i++) {

		tipo=netlist[i].nome[0];
    if (tipo=='R' || tipo=='C') {
		if (tipo=='R')
			g=1/netlist[i].par1;
		else{
			if (curTime==0.0){
				if (operationPoint)
					g=netlist[i].par1/(deltaT*1e10);
				else
					g=netlist[i].par1/(deltaT*1e-10);
/*				if (operationPoint)
					g=netlist[i].par1/1e10;
				else
					g=netlist[i].par1/1e-10;
*/			}
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
				if (i != (*curS).elNE){
					printf("Um error interno ocorreu\n");
					exit(1);
				}
				if (curTime-netlist[i].par4 < 0.0|| netlist[i].par8 == 0)/*numCiclos*/
					g=netlist[i].par1;
				else
					g=netlist[i].par1+netlist[i].par2*(exp(-netlist[ne].par5*
				(curTime-netlist[ne].par4)))*sin(2*PI*(netlist[i].par3)*(curTime-
				netlist[ne].par4)-((PI/180)*netlist[i].par6));
				curS=(*curS).next;
				break;
			case 'P':
				if (i != (*curD).elNE){
					printf("Um error interno ocorreu\n");
					exit(1);
				}
				g=0;
				if (curTime-netlist[i].par3 >= 0.0 && netlist[i].par9>0){
					double curTimeChecker=(*curD).internTimer-(netlist[i].par4+netlist[i].par7*(netlist[i].par8-netlist[i].par9));
					if (curTimeChecker<curTimePrecision && curTimeChecker<-deltaT*1e-5)
						g=netlist[i].par1+(((*curD).internTimer-netlist[i].par7*(netlist[i].par8-netlist[i].par9))*
							((netlist[i].par2-netlist[i].par1)/netlist[i].par4));
					else{
						curTimeChecker=(*curD).internTimer-(netlist[i].par4+netlist[i].par6+
						netlist[i].par7*(netlist[i].par8-netlist[i].par9));
						if (curTimeChecker<=(curTimePrecision+(deltaT*1e-5)))
							g=netlist[i].par2;
						else{
							curTimeChecker=(*curD).internTimer-(netlist[i].par4+netlist[i].par5+netlist[i].par6+
							netlist[i].par7*(netlist[i].par8-netlist[i].par9));
							if (curTimeChecker<=(curTimePrecision+(deltaT*1e-5)))
									g=netlist[i].par2-(((*curD).internTimer-(netlist[i].par4+netlist[i].par6+netlist[i].par7*
									(netlist[i].par8-netlist[i].par9)))*((netlist[i].par2-netlist[i].par1)/netlist[i].par5));
							else
								g=netlist[i].par1;
						}
					}
				}
				else
					g=netlist[i].par1;							/*desligado fica na amplitude inicial*/
				if ((*curD).internTimer+deltaT+(deltaT*1e-5) >= netlist[i].par7*(1+netlist[i].par8-netlist[i].par9))
					(*curD).renovar=verdadeiro;
				curD=(*curD).next;
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
					if (i != (*curS).elNE){
						printf("Um error interno ocorreu\n");
						exit(1);
					}
					if ((curTime-netlist[i].par4 < (-deltaT*1e-5)) || netlist[i].par8 == 0)/*numCiclos*/
						g=netlist[i].par1;
					else
						g=netlist[i].par1+(netlist[i].par2*(exp(-netlist[ne].par5*
						(curTime-netlist[ne].par4)))*sin(2*PI*(netlist[i].par3)*(curTime-netlist[ne].par4)-((PI/180)*netlist[i].par6)));
					curS=(*curS).next;
					break;
				case 'P':
					if (i != (*curD).elNE){
						printf("Um error interno ocorreu\n");
						exit(1);
					}
					if (curTime-netlist[i].par3 >= 0.0 && netlist[i].par9>0){
						double curTimeChecker=(*curD).internTimer-(netlist[i].par4+netlist[i].par7*(netlist[i].par8-netlist[i].par9));
						if (curTimeChecker<curTimePrecision && curTimeChecker<-deltaT*1e-5)
							g=netlist[i].par1+(((*curD).internTimer-netlist[i].par7*(netlist[i].par8-netlist[i].par9))*
								((netlist[i].par2-netlist[i].par1)/netlist[i].par4));
						else{
							curTimeChecker=(*curD).internTimer-(netlist[i].par4+netlist[i].par6+
							netlist[i].par7*(netlist[i].par8-netlist[i].par9));
							if (curTimeChecker<=(curTimePrecision+(deltaT*1e-5)))
								g=netlist[i].par2;
							else{
								curTimeChecker=(*curD).internTimer-(netlist[i].par4+netlist[i].par5+netlist[i].par6+
								netlist[i].par7*(netlist[i].par8-netlist[i].par9));
								if (curTimeChecker<=(curTimePrecision+(deltaT*1e-5)))
										g=netlist[i].par2-(((*curD).internTimer-(netlist[i].par4+netlist[i].par6+netlist[i].par7*
										(netlist[i].par8-netlist[i].par9)))*((netlist[i].par2-netlist[i].par1)/netlist[i].par5));
								else
									g=netlist[i].par1;
							}
						}
					}
					else
						g=netlist[i].par1;							/*desligado fica na amplitude inicial*/
					if ((*curD).internTimer+deltaT+(deltaT*1e-5) >= netlist[i].par7*(1+netlist[i].par8-netlist[i].par9))
						(*curD).renovar=verdadeiro;
					curD=(*curD).next;
					break;
				default:
					g=netlist[i].par1;
			}
			insertVolt:
				Yn[netlist[i].x][nv+1]-=g;
		}
		else{
			if (curTime==0.0){
				if (operationPoint)
					g=netlist[i].par1/(deltaT*1e10);
				else
					g=netlist[i].par1/(deltaT*1e-10);
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
		if (maxNR<=1)
			maxNR=50;
		switch (tipo) {
			case (')'):
			case ('('):
				voltage1=lastValues[netlist[i].a];
				voltage2=lastValues[netlist[i].b];
				break;
			case ('{'):
			case ('}'):
				voltage1=lastValues[netlist[i].b];
				voltage2=lastValues[netlist[i].a];
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
		if (curTime==0.0){
			if (operationPoint)
				g=netlist[i].par3/1e10;
			else
				if (deltaT<1e-8)
					g=netlist[i].par3/(deltaT*1e-10);
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
		if (i != (*curFF).elNE){
			printf("Um error interno ocorreu\n");
			exit(1);
		}
		if (netlist[i].auxComp){
			if (lastValues[netlist[i].auxComp->b] >=
				(netlist[i].auxComp->par1/2)){
					Qplus=0.0;
					Qminus=netlist[i].par1;
					(*curFF).high=falso;
			}
			else if (lastValues[netlist[i].auxComp->a] >=
						(netlist[i].auxComp->par1/2)){
					Qminus=0.0;
					Qplus=netlist[i].par1;
					(*curFF).high=verdadeiro;
			}
			else goto flipFlop;
		}
		else{
			flipFlop:
			if ((lastValues[netlist[i].d] >= (netlist[i].par1/2)) &&
				(lastLastValues[netlist[i].d] <= (netlist[i].par1/2))){
				if ((lastValues[netlist[i].c]+1e-10) >= (netlist[i].par1/2)){
					(*curFF).high=verdadeiro;
				}
				else{
					(*curFF).high=falso;
				}
			}
			else if (curTime<=0.0){
				Qplus=0.0;
				Qminus=netlist[i].par1;
			}
			else{
				if ((*curFF).high){
					Qplus=netlist[i].par1;
					Qminus=0.0;
				}
				else{
					Qplus=0.0;
					Qminus=netlist[i].par1;
				}
			}
		}
		g=1/netlist[i].par2;
		Yn[netlist[i].a][netlist[i].a]+=g;
		Yn[netlist[i].b][netlist[i].b]+=g;
		Yn[netlist[i].a][nv+1]+=g*Qplus;
		Yn[netlist[i].b][nv+1]+=g*Qminus;
		/*acrescentar os capacitores para terra*/
		if (curTime==0.0){
			if (operationPoint)
					g=netlist[i].par3/1e10;
			else{
				if (deltaT<1e-8)
					g=netlist[i].par3/(deltaT*1e-10);
				else
					g=netlist[i].par3/1e-10;
			}
		}
		else
			g=netlist[i].par3/deltaT;
		Yn[netlist[i].c][netlist[i].c]+=g;
		Yn[netlist[i].d][netlist[i].d]+=g;
		g=(g*lastValues[netlist[i].c]);	/*verificar*/
		Yn[netlist[i].c][nv+1]+=g;
		if (curTime==0.0){
			if (operationPoint)
					g=netlist[i].par3/1e10;
			else{
				if (deltaT<1e-8)
					g=netlist[i].par3/(deltaT*1e-10);
				else
					g=netlist[i].par3/1e-10;
			}
		}
		else
			g=netlist[i].par3/deltaT;
		g=(g*lastValues[netlist[i].d]);
		Yn[netlist[i].d][nv+1]+=g;

		/*capacitores do reset*/
		if (netlist[i].auxComp){
			if (curTime==0.0){
				if (operationPoint)
					g=netlist[i].par3/1e10;
				else{
					if (deltaT<1e-8)
						g=netlist[i].par3/(deltaT*1e-10);
					else
						g=netlist[i].par3/1e-10;
				}
			}
			else
				g=netlist[i].auxComp->par2;
			Yn[netlist[i].auxComp->a][netlist[i].auxComp->a]+=g;
			Yn[netlist[i].auxComp->b][netlist[i].auxComp->b]+=g;
			g=(g*lastValues[netlist[i].auxComp->a]);	/*verificar*/
			Yn[netlist[i].auxComp->a][nv+1]+=g;
			g=(g*lastValues[netlist[i].auxComp->b]);	/*verificar*/
			Yn[netlist[i].auxComp->b][nv+1]+=g;
		}
		curFF=(*curFF).next;
	}

	else if (tipo=='@'){/*monoestavel*/
		double Qplus, Qminus;
		if (i != (*curM).elNE){
			printf("Um error interno ocorreu\n");
			exit(1);
		}
		if (lastValues[netlist[i].d] >= netlist[i].par1/2){
			Qplus=0.0;
			Qminus=netlist[i].par1;
			(*curM).up=falso;
		}
		if (!(*curM).up){
			if ((lastValues[netlist[i].c]>=netlist[i].par1/2) &&
					(lastLastValues[netlist[i].c] < netlist[i].par1/2)){
					Qplus=netlist[i].par1;
					Qminus=0.0;
					(*curM).turnDown=curTime+netlist[i].par4;
					(*curM).up=verdadeiro;
			}
			else{
				Qplus=0.0;
				Qminus=netlist[i].par1;
			}
		}
		else{
			if (curTime-(*curM).turnDown > 0.0+deltaT*1e-5){
				Qplus=0.0;
				Qminus=netlist[i].par1;
				(*curM).up=falso;
			}
			else{
				Qplus=netlist[i].par1;
				Qminus=0.0;
			}
		}
		g=1/netlist[i].par2;
		Yn[netlist[i].a][netlist[i].a]+=g;
		Yn[netlist[i].b][netlist[i].b]+=g;
		Yn[netlist[i].a][nv+1]+=g*Qplus;
		Yn[netlist[i].b][nv+1]+=g*Qminus;
		/*acrescentar os capacitores para terra*/
		if (curTime==0.0){
			if (operationPoint)
				g=netlist[i].par3/1e10;
			else{
				if (deltaT<1e-8)
					g=netlist[i].par3/(deltaT*1e-10);
				else
					g=netlist[i].par3/1e-10;
			}
		}
		else
			g=netlist[i].par3/deltaT;
		Yn[netlist[i].c][netlist[i].c]+=g;
		Yn[netlist[i].d][netlist[i].d]+=g;
		g=(g*lastValues[netlist[i].c]);	/*verificar*/
		g=(g*lastValues[netlist[i].d]);	/*verificar*/
		Yn[netlist[i].d][nv+1]+=g;
		curM=(*curM).next;
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

	if (maxNR > 1){
		for (j=1; j<nv; j++)			/*newton raphson*/
			if (nrValues[j]!=Yn[j][nv+1]){
				lastNRValues[j]=nrValues[j];
				needNR=verdadeiro;
				nrValues[j]=Yn[j][nv+1];
			}
	}
			else{
				for (j=1;j<nv;j++){
					lastLastValues[j]=lastValues[j];
					lastValues[j]=Yn[j][nv+1];
				}
			}

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
	if (curTime==0.0){
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
		if (i==1) fprintf(tabela,"%g ",curTime);
		if (i!=nv)
			fprintf(tabela,"%g ",Yn[i][nv+1]);
		else{
			if (curTime <= tFinal)
				fprintf(tabela,"%g\n",Yn[i][nv+1]);
			else
				fprintf(tabela,"%g",Yn[i][nv+1]);
		}
	}
  getch();
	if (needBE)
		if (curTime<=tFinal){
			cur=first;
			while (cur){
				if (netlist[(*cur).elNE].nome[0]=='C'){
					if (netlist[(*cur).elNE].b==0)
						netlist[(*cur).elNE].par2=Yn[netlist[(*cur).elNE].a][nv+1];
					else
						netlist[(*cur).elNE].par2=Yn[netlist[(*cur).elNE].a][nv+1]-Yn[netlist[(*cur).elNE].b][nv+1];
/*					printf("%g-%g\n",Yn[netlist[((*cur).elNE)].a][nv+1],Yn[netlist[(*cur).elNE].b][nv+1]);
					getchar();
*/					}
				else{
					netlist[(*cur).elNE].par2=Yn[netlist[(*cur).elNE].x][nv+1];
/*					printf("%g\n",Yn[netlist[(*cur).elNE].x][nv+1]);
					getchar();*/
					}
				cur=(*cur).next;
			}
			curD=firstD;
			while (curD){
				if (netlist[(*curD).elNE].par3-curTime < 0)
					(*curD).internTimer+=deltaT;
				if ((*curD).renovar && (((*curD).internTimer - (netlist[(*curD).elNE].par7-deltaT)*(1+netlist[(*curD).elNE].par8-
					netlist[(*curD).elNE].par9)) >= curTimePrecision)){
					(*curD).renovar=falso;
					netlist[(*curD).elNE].par9--;
				}
				curD=(*curD).next;
			}
			curS=firstS;
			while(curS){
				if ((*curS).curPer-curTime < 0.0-(deltaT*1e-5))
					if (netlist[(*curS).elNE].par8 > 0){
						netlist[(*curS).elNE].par8--;
						(*curS).curPer+=(1/netlist[(*curS).elNE].par3);
					}
				curS=(*curS).next;
			}

			if (maxNR>1){
				for (i=0;i<nv;i++){
					lastLastValues[i]=lastValues[i];
					lastLastValues[i]=nrValues[i];
				}
			}
//			else{
//				for (j=1;j<nv;j++){
//					lastLastValues[j]=lastValues[j];
//					lastValues[j]=Yn[j][nv+1];
//				}
//			}

			if (curTime>=deltaT)
				for (i=0; i<nv; i++)
					lastLastValues[i]=lastValues[i];
			for (i=0; i<nv; i++)
				lastValues[i]=nrValues[i];

			lastTime=curTime;
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
