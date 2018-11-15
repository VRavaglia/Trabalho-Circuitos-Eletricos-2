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
#define TOLG 1e-9
#define STDDETAT 0.1
#define NUMINTER 20
#define PI 3.14159265359
/*#define DEBUG*/

typedef enum boolean {falso=0, verdadeiro} boolean;

typedef struct elemento { /* Elemento do netlist */
	char nome[MAX_NOME],fType;			/*fType usado apenas por fontes*/
	double par1, par2, par3, par4, par5, par6, par7, par8;
	/*parametros na ordem em que aparecem na netlist*/
	int a,b,c,d,x,y;			/*nos (x,y sao para controle)*/
} elemento;

typedef struct tVarCell{
	int elNE;					/*guarda o numero do elemente variante no tempo*/
	struct tVarCell *next;
} tVarCell;

typedef struct dDCell{
	int elNE;					/*guarda o numero da fonte que varia no tempo (nao
										senoidal*/
	struct dDCell *next;
	boolean triggered;
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

FILE *arquivo;

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
	double time=-(STDDETAT);
	tVarCell *first;
	dDCell *firstD;
	char string[MAX_NOME+1];
	boolean operationPoint=falso;

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
					dDCell *curD;
					curD = (dDCell *) malloc(sizeof(dDCell));
					(*curD).elNE=ne;
					(*curD).next= (dDCell *) NULL;
					(*curD).triggered=falso;
					if (!firstD) firstD=curD;
					else{
						dDCell *auxD;
						auxD=firstD;
						while((*auxD).next) auxD=(*auxD).next;
						(*auxD).next=curD;
					}
					sscanf(p, "%10s%10s%10s%lg%lg%lg%lg%lg%lg%lg%lg",na,nb,string,
							&netlist[ne].par1,&netlist[ne].par2,&netlist[ne].par3,
							&netlist[ne].par4,&netlist[ne].par5,&netlist[ne].par6,
							&netlist[ne].par7,&netlist[ne].par8);
					printf("%s %s %s %s %g %g %g %g %g %g %g %g\n", netlist[ne].nome,na,nb,
						string,netlist[ne].par1,netlist[ne].par2,netlist[ne].par3,
						netlist[ne].par4,netlist[ne].par5,netlist[ne].par6,
						netlist[ne].par7,netlist[ne].par8);
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
		tVarCell *cur;
		cur = (tVarCell *) malloc(sizeof(tVarCell));
		(*cur).elNE=ne;
		(*cur).next= (tVarCell *) NULL;
		if (!first) first=cur;
		else{
			tVarCell *aux;
			aux=first;
			while((*aux).next) aux=(*aux).next;
			(*aux).next=cur;
		}
		sscanf(p,"%10s%10s%lg%lg",na,nb,&netlist[ne].par1,&netlist[ne].par2);
		printf("%s %s %s %g %g\n",netlist[ne].nome,na,nb,netlist[ne].par1,netlist[ne].par2);
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
  /* Lista tudo */
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
		else
			printf("%s %d %d %s %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,string,
			netlist[i].par1);
	}
	else if (tipo=='C' || tipo=='L')
		printf("%s %d %d %g %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,
				netlist[i].par1,netlist[i].par2);
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

	buildSys:
	time+=STDDETAT;

	{
		dDCell *curD, *earlier;
		earlier = curD = (dDCell *) NULL;

		if (firstD) curD=firstD;
		while(curD){
			if (((netlist[(*curD).elNE].par3) <= time) &&
				!((*curD).triggered)){
					time=netlist[(*curD).elNE].par3;
					earlier=curD;
			}
			curD=(*curD).next;
		}
		if (earlier) (*earlier).triggered=verdadeiro;
	}

  /* Zera sistema */
  for (i=0; i<=nv; i++) {
    for (j=0; j<=nv+1; j++)
      Yn[i][j]=0;
  }
  /* Monta estampas */
  for (i=1; i<=ne; i++) {

		dDCell *curD;
		if (firstD) curD=firstD;

		tipo=netlist[i].nome[0];
    if (tipo=='R' || tipo=='C') {
		/*provavel erro aqui */
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
			g=(netlist[i].par2/g);	/*verificar*/
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
					if (netlist[i].par3+netlist[i].par4 > time)
						g=netlist[i].par1+(((netlist[i].par1-netlist[i].par2)/
						netlist[i].par4)*(time-netlist[i].par3));
					else if (netlist[i].par3+netlist[i].par4+netlist[i].par6 >=
								time)
						g=netlist[i].par2;
					else if (netlist[i].par3+netlist[i].par4+netlist[i].par6+
								netlist[i].par5 >= time){
						g=netlist[i].par2-(((netlist[i].par2-netlist[i].par1)/
						netlist[i].par5)*(time-(netlist[i].par3+netlist[i].par4+
						netlist[i].par6)));
						if (time==netlist[i].par3+netlist[i].par4+netlist[i].par6+
							netlist[i].par5) goto renew;
					}
					else{
						if (netlist[i].par3+netlist[i].par7 > time || netlist[i].par8==0){
							(*curD).triggered=falso;
							g=netlist[i].par1;					/*desligado fica na amplitude inicial*/
						}
						renew:
							netlist[i].par3+=netlist[i].par7;
							if (netlist[i].par8 > 0) netlist[i].par8--;
					}
					curD=(*curD).next;
				}
				else g=netlist[i].par1;							/*desligado fica na amplitude inicial*/
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
						if (netlist[i].par3+netlist[i].par4 > time)
							g=netlist[i].par1+(((netlist[i].par1-netlist[i].par2)/
							netlist[i].par4)*(time-netlist[i].par3));
						else if (netlist[i].par3+netlist[i].par4+netlist[i].par6 >=
									time)
							g=netlist[i].par2;
						else if (netlist[i].par3+netlist[i].par4+netlist[i].par6+
									netlist[i].par5 >= time){
							g=netlist[i].par2-(((netlist[i].par2-netlist[i].par1)/
							netlist[i].par5)*(time-(netlist[i].par3+netlist[i].par4+
							netlist[i].par6)));
							if (time==netlist[i].par3+netlist[i].par4+netlist[i].par6+
								netlist[i].par5) goto renew2;
						}
						else{
							if (((netlist[i].par3+netlist[i].par7) > time) ||
							netlist[i].par8==0) g=netlist[i].par1;
							/*desligado fica na amplitude inicial*/
							renew2:
								(*curD).triggered=falso;
								netlist[i].par3+=netlist[i].par7;
								if (netlist[i].par8 > 0) netlist[i].par8--;
						}
							curD=(*curD).next;
					}
					else g=netlist[i].par1;							/*desligado fica na amplitude inicial*/
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
  /* Mostra solucao */
	if (needBE){
		printf("\nSolucao(#%i):\n", ++iTemp);
		printf("t=%g\n", time);
	}
	else
		printf("\nSolucao:\n");
  strcpy(txt,"Tensao");
  for (i=1; i<=nv; i++) {
    if (i==nn+1) strcpy(txt,"Corrente");
    printf("%s %s: %g\n",txt,lista[i],Yn[i][nv+1]);
  }
  getch();
	if (needBE)
		if (iTemp<NUMINTER){
			tVarCell *cur;
			cur=first;
			while (cur){
				if (netlist[(*cur).elNE].nome[0]=='C')
					netlist[(*cur).elNE].par2=Yn[netlist[(*cur).elNE].a][nv+1]-Yn[netlist[(*cur).elNE].b][nv+1];
				else
					netlist[(*cur).elNE].par2=Yn[netlist[(*cur).elNE].x][nv+1];
				cur=(*cur).next;
			}
			goto buildSys;
		}
		else
		{
			tVarCell *cur,*aux;
			cur=first;
			while(cur){
				aux=(*cur).next;
				free(cur);
				cur=aux;
			}
			dDCell *curD,*auxD;
			curD=firstD;
			while(curD){
				auxD=(*curD).next;
				free(curD);
				curD=auxD;
			}
		}
  return 0;
}
