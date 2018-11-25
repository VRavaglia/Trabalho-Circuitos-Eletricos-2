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
#include <iostream>

#include <string>
#include <stdio.h>

/*#include <conio.h>*/
#include "Interface4Main.h"

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


using namespace std;

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
	bool triggered,renovar;
} dDCell;

typedef struct dSCell{	/*guarda o numero da fonte senoidal*/
	int elNE;
	double curPer;
	struct dSCell *next;
} dSCell;

typedef struct mCell{	/*guarda quando o monoestavel deve ser desligado*/
	int elNE;
	double turnDown;
	struct mCell *next;
	bool up;
} mCell;

elemento netlist[MAX_ELEM+1]; /* Netlist */

int
  ne, /* Elementos */
  nv, /* Variaveis */
  nn, /* Nos */
  i,j,k;

bool needBE=false;

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
int resolversistema(Interface4Frame& frame)
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
      frame.i_erro("Sistema singular\n");
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
int numero(char *nome, Interface4Frame& frame)
{
  int i,achou;

  i=0; achou=0;
  while (!achou && i<=nv)
    if (!(achou=!strcmp(nome,lista[i]))) i++;
  if (!achou) {
    if (nv==MAX_NOS) {
      frame.i_erro("O programa so aceita ate %d nos\n",nv);
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

int calculo(const char *caminho_netlist, Interface4Frame &frame)
{
	int iTemp=0;											/*guarda o numero de interacoes no tempo*/
	unsigned iNR=0,maxNR=0, j;	/*guarda o numero de interacoes
															newton raphson*/
	double time,lastTime=0.0,tFinal=0.0,timePrecision;
	tVarCell *first, *cur, *aux;
	dDCell *firstD, *curD, *auxD, *earlier;
	dSCell *firstS, *curS, *auxS;
	mCell *firstM, *curM, *auxM;
	char string[MAX_NOME+1],icOpo[MAX_NOME+1];

	double *nrValues,*lastValues,*lastLastValues;/*guarda os valores
																anteriores/ultima
												interacao para o uso do metodo a analise
												de newton raphson respectivamente*/
	bool operationPoint=true,needNR=false;

	first=(tVarCell *) NULL;
	firstD=(dDCell *) NULL;
	firstS=(dSCell *) NULL;
	firstM=(mCell *) NULL;

  frame.i_printf("Programa demonstrativo de analise nodal modificada\n");
  frame.i_printf("Por Antonio Carlos M. de Queiroz - acmq@coe.ufrj.br\n");
  frame.i_printf("Versao %s\n",versao);

  /* Leitura do netlist */
  ne=0; nv=0; strcpy(lista[0],"0");

	arquivo = fopen(caminho_netlist, "r");

  if (arquivo == NULL) {

    frame.i_erro("Erro ao ler o arquivo");

    return -1;

  }
    std::string nomeTabela ="";

	nomeTabela = caminho_netlist;
	nomeTabela = nomeTabela.substr(0, nomeTabela.size() - 3);
	nomeTabela = nomeTabela + "tab";
	tabela=fopen(nomeTabela.c_str(),"w");

  frame.i_printf("Lendo netlist:\n");
  fgets(txt,MAX_LINHA,arquivo);
  frame.i_printf("Titulo: %s",txt);
  while (fgets(txt,MAX_LINHA,arquivo)) {
    ne++; /* Nao usa o netlist[0] */
    if (ne>MAX_ELEM) {
      frame.i_erro("O programa so aceita ate %d elementos.\n",MAX_ELEM);
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
					frame.i_printf("%s %s %s %s %g %g %g %g %g %g %u\n", netlist[ne].nome,na,nb,
					string,netlist[ne].par1,netlist[ne].par2,netlist[ne].par3,
					netlist[ne].par4,netlist[ne].par5,netlist[ne].par6,netlist[ne].par8);
					netlist[ne].par9=netlist[ne].par8;
					if (!needBE) needBE=true;
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
					if (!needBE) needBE=true;
					curD = (dDCell *) malloc(sizeof(dDCell));
					(*curD).elNE=ne;
					(*curD).next=(dDCell *) NULL;
					(*curD).internTimer=0.0;
					(*curD).triggered=false;
					(*curD).renovar=false;
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
					frame.i_printf("%s %s %s %s %g %g %g %g %g %g %g %u\n", netlist[ne].nome,na,nb,
						string,netlist[ne].par1,netlist[ne].par2,netlist[ne].par3,
						netlist[ne].par4,netlist[ne].par5,netlist[ne].par6,
						netlist[ne].par7,netlist[ne].par8);
						netlist[ne].par9=netlist[ne].par8;
					break;
				default:
					sscanf(p,"%10s%10s%10s%lg",na,nb,string,&netlist[ne].par1);
					frame.i_printf("%s %s %s %s %g\n",netlist[ne].nome,na,nb,string,netlist[ne].par1);
			}
		}
		else{
			sscanf(p,"%10s%10s%lg",na,nb,&netlist[ne].par1);
			frame.i_printf("%s %s %g\n",netlist[ne].nome,na,nb,netlist[ne].par1);
		}
      netlist[ne].a=numero(na, frame);
      netlist[ne].b=numero(nb, frame);
    }

    else if (tipo=='G' || tipo=='E' || tipo=='F' || tipo=='H') {
      sscanf(p,"%10s%10s%10s%10s%lg",na,nb,nc,nd,&netlist[ne].par1);
      frame.i_printf("%s %s %s %s %s %g\n",netlist[ne].nome,na,nb,nc,nd,netlist[ne].par1);
      netlist[ne].a=numero(na, frame);
      netlist[ne].b=numero(nb, frame);
      netlist[ne].c=numero(nc, frame);
      netlist[ne].d=numero(nd, frame);
    }
	else if (tipo=='C' || tipo=='L'){
		if (!needBE) needBE=true;
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
		frame.i_printf("%s %s %s %g %g\n",netlist[ne].nome,na,nb,netlist[ne].par1,netlist[ne].par2);
		netlist[ne].a=numero(na, frame);
		netlist[ne].b=numero(nb, frame);
	}

	else if (tipo==')'||tipo=='('||tipo=='}'||tipo=='{'){/*portas*/
		sscanf(p,"%10s%10s%10s%lg%lg%lg%lg",na,nb,nc,&netlist[ne].par1,
				&netlist[ne].par2,&netlist[ne].par3,&netlist[ne].par4);/*par4 == A*/
		frame.i_printf("%s %s %s %s %g %g %g %g\n",netlist[ne].nome,na,nb,nc,
				netlist[ne].par1,netlist[ne].par2,netlist[ne].par3,
				netlist[ne].par4);
		netlist[ne].a=numero(na, frame);
		netlist[ne].b=numero(nb, frame);
		netlist[ne].c=numero(nc, frame);
	}
	else if (tipo=='%'){/*flip-flop*/
		char auxComp[MAX_NOME];
		sscanf(p,"%10s%10s%10s%10s%10s%lg%lg%lg",na,nb,nc,nd,
				auxComp,&netlist[ne].par1,&netlist[ne].par2,
				&netlist[ne].par3);
		frame.i_printf("%s %s %s %s %s %g %g %g\n",netlist[ne].nome,na,nb,nc,nd,
				auxComp,netlist[ne].par1,netlist[ne].par2,
				netlist[ne].par3);
		netlist[ne].a=numero(na, frame);
		netlist[ne].b=numero(nb, frame);
		netlist[ne].c=numero(nc, frame);
		netlist[ne].d=numero(nd, frame);
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
		(*curM).elNE=ne;
		(*curM).next=(mCell *) NULL;
		(*curM).up=false;
		if (!firstM) firstM=curM;
		else{
			auxM=firstM;
			while((*auxM).next) auxM=(*auxM).next;
			(*auxM).next=curM;
		}
		curM=firstM;
		frame.i_printf("%s %s %s %s %g %g %g %g\n",netlist[ne].nome,na,nb,nc,nd,
				netlist[ne].par1,netlist[ne].par2,netlist[ne].par3,
				netlist[ne].par4);
		netlist[ne].a=numero(na, frame);
		netlist[ne].b=numero(nb, frame);
		netlist[ne].c=numero(nc, frame);
		netlist[ne].d=numero(nd, frame);
	}
	else if (tipo=='!'){
		sscanf(p,"%10s%10s%lg%lg",na,nb,&netlist[ne].par1,&netlist[ne].par2);
		frame.i_printf("%s %s %s %g %g\n",na,nb,&netlist[ne].par1,&netlist[ne].par2);
		netlist[ne].a=numero(na, frame);
		netlist[ne].b=numero(nb, frame);
	}
    else if (tipo=='O') {
      sscanf(p,"%10s%10s%10s%10s",na,nb,nc,nd);
      frame.i_printf("%s %s %s %s %s\n",netlist[ne].nome,na,nb,nc,nd);
      netlist[ne].a=numero(na, frame);
      netlist[ne].b=numero(nb, frame);
      netlist[ne].c=numero(nc, frame);
      netlist[ne].d=numero(nd, frame);
    }
	else if (tipo=='.'){
		char analysisTipe[MAX_NOME];
		strcpy(icOpo,"\0");
		sscanf(p,"%lg%lg%10s%u%10s",&tFinal,&deltaT,analysisTipe,&maxNR,icOpo);
		if (strlen(icOpo))
			if (toupper(icOpo[1]=='I'))
				operationPoint=false;
		frame.i_printf("%s %g %g %s %u %s\n",netlist[ne].nome,tFinal,deltaT,analysisTipe,
												maxNR,icOpo);
		ne--;
	}
    else if (tipo=='*') { /* Comentario comeca com "*" */
      frame.i_printf("Comentario: %s",txt);
      ne--;
    }
    else {
      frame.i_erro("Elemento desconhecido: %s\n",txt);

      exit(1);
    }
  }
  fclose(arquivo);
	if (tFinal==0.0){
		//Pede os parametros necessarios para a simulacao
		//Primeiramente, exibe uma caixa de dialogo para receber o tempo inicial

		//Exibe uma caixa de dialogo para receber o tempo final
		wxTextEntryDialog inputDialog2(&frame, _("Entre com o tempo final da simulacao:"), _("Simulacao"));
		if (inputDialog2.ShowModal() != wxID_OK) {
				frame.i_erro("Tempo inserido invalido.");
				return -1;
		}

			//Exibe uma caixa de dialogo para receber o tamanho do passo
		wxTextEntryDialog inputDialog3(&frame, _("Entre com tamanho do passo temporal:"), _("Simulacao"));
		if (inputDialog3.ShowModal() != wxID_OK) {
				frame.i_erro("Tempo inserido invalido.");
				return -1;
		}

		//Exibe uma caixa de dialogo para receber o numero de passos no NR
		wxTextEntryDialog inputDialog4(&frame, _("Entre com o numero de passos por ponto na tabela:"), _("Simulacao"));
		if (inputDialog4.ShowModal() != wxID_OK) {
				frame.i_erro("Numero inserido invalido.");
				return -1;
		}

			//Exibe uma caixa de dialogo com as opcoes de inicializacoes possiveis para que uma seja escolhida
		wxString escolhas_inicializacao[] = {_("Ponto de Operacao"), _("Condicoes Iniciais")};

		//Checa se alguma opcao foi selecionada
		wxSingleChoiceDialog  inputDialog1(&frame, _("Selecione o tipo de inicializacao da simulacao"), _("Simulacao"), 2, escolhas_inicializacao);
		if (inputDialog1.ShowModal() != wxID_OK) {
				frame.i_erro("Tipo de inicializacao nao selecionado");
				return -1;
		}

		//A partir dos dialogos, gera os parametros que vao ser passados como argumentos pra funcao principal
		if (inputDialog1.GetSelection() == 1){
				operationPoint = false;
		}
		else{
				operationPoint = true;
		}

		tFinal = atof(inputDialog2.GetValue().mb_str());
		deltaT = atof(inputDialog3.GetValue().mb_str());
		maxNR = atof(inputDialog4.GetValue().mb_str());
	}
	time=-deltaT;
	timePrecision=deltaT/2;
  /* Acrescenta variaveis de corrente acima dos nos, anotando no netlist */
  nn=nv;
  for (i=1; i<=ne; i++) {
    tipo=netlist[i].nome[0];
    if (tipo=='V' || tipo=='E' || tipo=='F' || tipo=='O' || tipo=='L') {
      nv++;
      if (nv>MAX_NOS) {
        frame.i_printf("As correntes extra excederam o numero de variaveis permitido (%d)\n",MAX_NOS);
        exit(1);
      }
      strcpy(lista[nv],"j"); /* Tem espaco para mais dois caracteres */
      strcat(lista[nv],netlist[i].nome);
      netlist[i].x=nv;
    }
    else if (tipo=='H') {
      nv=nv+2;
      if (nv>MAX_NOS) {
        frame.i_printf("As correntes extra excederam o numero de variaveis permitido (%d)\n",MAX_NOS);
        exit(1);
      }
      strcpy(lista[nv-1],"jx"); strcat(lista[nv-1],netlist[i].nome);
      netlist[i].x=nv-1;
      strcpy(lista[nv],"jy"); strcat(lista[nv],netlist[i].nome);
      netlist[i].y=nv;
    }
  }
	itsCur:

  frame.i_printf("Variaveis internas: \n");
  for (i=0; i<=nv; i++)
    frame.i_printf("%d -> %s\n",i,lista[i]);

  frame.i_printf("Netlist interno final\n");
  for (i=1; i<=ne; i++) {
    tipo=netlist[i].nome[0];
	if (tipo=='R')
		frame.i_printf("%s %d %d %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].par1);
	if (tipo=='I' || tipo=='V'){
		if (netlist[i].fType=='S'){
			strcpy(string,"SIN");
			frame.i_printf("%s %d %d %s %g %g %g %g %g %g %g\n", netlist[i].nome,netlist[i].a,
			netlist[i].b,string,netlist[i].par1,netlist[i].par2,netlist[i].par3,
			netlist[i].par4,netlist[i].par5,netlist[i].par6, netlist[i].par7);
		}
		else if (netlist[i].fType=='P'){
			strcpy(string,"PULSE");
			frame.i_printf("%s %d %d %s %g %g %g %g %g %g %g %u\n", netlist[i].nome,netlist[i].a,
			netlist[i].b,string,netlist[i].par1,netlist[i].par2,netlist[i].par3,
			netlist[i].par4,netlist[i].par5,netlist[i].par6, netlist[i].par7,
			netlist[i].par8);
		}
		else{
			strcpy(string,"DC");
			frame.i_printf("%s %d %d %s %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,string,
			netlist[i].par1);
		}
	}
	else if (tipo=='C' || tipo=='L')
		frame.i_printf("%s %d %d %g %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,
				netlist[i].par1,netlist[i].par2);
	else if (tipo==')'||tipo=='('||tipo=='{'||tipo=='}')
		frame.i_printf("%s %d %d %g %g %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,
				netlist[i].par1,netlist[i].par2,netlist[i].par3);
    else if (tipo=='G' || tipo=='E' || tipo=='F' || tipo=='H') {
      frame.i_printf("%s %d %d %d %d %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].c,netlist[i].d,netlist[i].par1);
    }
    else if (tipo=='O') {
      frame.i_printf("%s %d %d %d %d\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].c,netlist[i].d);
    }
    if (tipo=='V' || tipo=='E' || tipo=='F' || tipo=='O' || tipo=='L')
      frame.i_printf("Corrente jx: %d\n",netlist[i].x);
    else if (tipo=='H')
      frame.i_printf("Correntes jx e jy: %d, %d\n",netlist[i].x,netlist[i].y);
  }

  /* Monta o sistema nodal modificado */
  frame.i_printf("O circuito tem %d nos, %d variaveis e %d elementos\n",nn,nv,ne);


	nrValues=(double *) malloc(sizeof(double)*(nv));
	lastValues=(double *) malloc(sizeof(double)*(nv));
	lastLastValues=(double *) malloc(sizeof(double)*(nv));

	for (i=0; i<nv; i++)
		nrValues[i]=lastValues[i]=lastLastValues[i]=0.0;

	buildSys:
	time+=deltaT;

	curD=firstD;
	while(curD){
		if (((netlist[(*curD).elNE].par3) <= time) &&
			!((*curD).triggered)){
				(*curD).triggered=true;
				(*curD).internTimer=deltaT+(time-netlist[(*curD).elNE].par3);
		}
		curD=(*curD).next;
	}

	newtonRaphson:
	needNR=false;

	if (++iNR>MAXNR){
		frame.i_printf("Sistema não convergiu\n");
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
	if (firstS) curS=firstS;
	if (firstM) curM=firstM;

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
				if (i != (*curS).elNE){
					frame.i_erro("Um error interno ocorreu\n");
					exit(1);
				}
				if (time-netlist[i].par4 < 0.0|| netlist[i].par8 == 0)/*numCiclos*/
					g=netlist[i].par1;
				else
					g=netlist[i].par1+netlist[i].par2*(exp(-netlist[ne].par5*
				(time-netlist[ne].par4)))*sin(2*PI*(netlist[i].par3)*(time-
				netlist[ne].par4)-((PI/180)*netlist[i].par6));
				curS=(*curS).next;
				break;
			case 'P':
				if (i != (*curD).elNE){
					frame.i_erro("Um error interno ocorreu\n");
					exit(1);
				}
				g=0;
				if (time-netlist[i].par3 >= 0.0 && netlist[i].par9>0){
					double timeChecker=(*curD).internTimer-(netlist[i].par4+netlist[i].par7*(netlist[i].par8-netlist[i].par9));
					if (timeChecker<timePrecision && timeChecker<-deltaT*1e-5)
						g=netlist[i].par1+(((*curD).internTimer-netlist[i].par7*(netlist[i].par8-netlist[i].par9))*
							((netlist[i].par2-netlist[i].par1)/netlist[i].par4));
					else{
						timeChecker=(*curD).internTimer-(netlist[i].par4+netlist[i].par6+
						netlist[i].par7*(netlist[i].par8-netlist[i].par9));
						if (timeChecker<=(timePrecision+(deltaT*1e-5)))
							g=netlist[i].par2;
						else{
							timeChecker=(*curD).internTimer-(netlist[i].par4+netlist[i].par5+netlist[i].par6+
							netlist[i].par7*(netlist[i].par8-netlist[i].par9));
							if (timeChecker<=(timePrecision+(deltaT*1e-5)))
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
					(*curD).renovar=true;
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
						frame.i_erro("Um error interno ocorreu\n");
						exit(1);
					}
					if (time-netlist[i].par4 < 0.0 || netlist[i].par8 == 0)/*numCiclos*/
						g=netlist[i].par1;
					else
						g=netlist[i].par1+netlist[i].par2*(exp(-netlist[ne].par5*
						(time-netlist[ne].par4)))*sin(2*PI*(netlist[i].par3)*(time-
						netlist[ne].par4)-((PI/180)*netlist[i].par6));
					curS=(*curS).next;
					break;
				case 'P':
					if (i != (*curD).elNE){
						frame.i_erro("Um error interno ocorreu\n");
						exit(1);
					}
					if (time-netlist[i].par3 >= 0.0 && netlist[i].par9>0){
						double timeChecker=(*curD).internTimer-(netlist[i].par4+netlist[i].par7*(netlist[i].par8-netlist[i].par9));
						if (timeChecker<timePrecision && timeChecker<-deltaT*1e-5)
							g=netlist[i].par1+(((*curD).internTimer-netlist[i].par7*(netlist[i].par8-netlist[i].par9))*
								((netlist[i].par2-netlist[i].par1)/netlist[i].par4));
						else{
							timeChecker=(*curD).internTimer-(netlist[i].par4+netlist[i].par6+
							netlist[i].par7*(netlist[i].par8-netlist[i].par9));
							if (timeChecker<=(timePrecision+(deltaT*1e-5)))
								g=netlist[i].par2;
							else{
								timeChecker=(*curD).internTimer-(netlist[i].par4+netlist[i].par5+netlist[i].par6+
								netlist[i].par7*(netlist[i].par8-netlist[i].par9));
								if (timeChecker<=(timePrecision+(deltaT*1e-5)))
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
						(*curD).renovar=true;
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

		/*capacitores do reset*/
		if (netlist[i].auxComp){
			if (time==0.0){
				if (operationPoint)
					g=netlist[i].par3/1e10;
				else
					g=netlist[i].par3/1e-10;
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
	}

	else if (tipo=='@'){/*monoestavel*/
		double Qplus, Qminus;
		if (i != (*curM).elNE){
			frame.i_erro("Um error interno ocorreu\n");
			exit(1);
		}
		if (lastValues[netlist[i].d] >= netlist[i].par1/2){
			Qplus=0.0;
			Qminus=netlist[i].par1;
			(*curM).up=false;
		}
		if (!(*curM).up){
			if ((lastValues[netlist[i].c]>=netlist[i].par1/2) &&
					(lastLastValues[netlist[i].c] < netlist[i].par1/2)){
					Qplus=netlist[i].par1;
					Qminus=0.0;
					(*curM).turnDown=time+netlist[i].par4;
					(*curM).up=true;
			}
			else{
				Qplus=0.0;
				Qminus=netlist[i].par1;
			}
		}
		else{
			if (time-(*curM).turnDown > 0.0+deltaT*1e-5){
				Qplus=0.0;
				Qminus=netlist[i].par1;
				(*curM).up=false;
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
		if (time==0.0){
			if (operationPoint)
				g=netlist[i].par3/1e10;
			else
				g=netlist[i].par3/1e-10;
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
    frame.i_printf("Sistema apos a estampa de %s\n",netlist[i].nome);
    for (k=1; k<=nv; k++) {
      for (j=1; j<=nv+1; j++)
        if (Yn[k][j]!=0) frame.i_printf("%+3.1f ",Yn[k][j]);
        else frame.i_printf(" ... ");
      frame.i_printf("\n");
    }

#endif
  }
  /* Resolve o sistema */
  if (resolversistema(frame)) {

    exit(1);
  }

	if (maxNR > 1)
		for (j=1; j<nv; j++)			/*newton raphson*/
			if (nrValues[j]!=Yn[j][nv+1]){
				needNR=true;
				nrValues[j]=Yn[j][nv+1];
			}

	if (needNR)
		goto newtonRaphson;
	iNR=0;

#ifdef DEBUG
  /* Opcional: Mostra o sistema resolvido */
  frame.i_printf("Sistema resolvido:\n");
  for (i=1; i<=nv; i++) {
      for (j=1; j<=nv+1; j++)
        if (Yn[i][j]!=0) frame.i_printf("%+3.1f ",Yn[i][j]);
        else frame.i_printf(" ... ");
      frame.i_printf("\n");
    }

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
				if (netlist[(*curD).elNE].par3-time < 0)
					(*curD).internTimer+=deltaT;
				if ((*curD).renovar && (((*curD).internTimer - (netlist[(*curD).elNE].par7-deltaT)*(1+netlist[(*curD).elNE].par8-
					netlist[(*curD).elNE].par9)) >= timePrecision)){
					(*curD).renovar=false;
					netlist[(*curD).elNE].par9--;
				}
				curD=(*curD).next;
			}
			curS=firstS;
			while(curS){
				if ((*curS).curPer-time < 0.0-(deltaT*1e-5))
					if (netlist[(*curS).elNE].par8 > 0){
						netlist[(*curS).elNE].par8--;
						(*curS).curPer+=(1/netlist[(*curS).elNE].par3);
					}
				curS=(*curS).next;
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
