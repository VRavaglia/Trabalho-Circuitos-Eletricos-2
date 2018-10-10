## Funções MNA1
*Variaveis:
	*v == numero de variaveis
	*Yn == matriz que contem os valores para a analise nodal modificada
	*ne == guarda o numero atual de elementos (tambem eh usado como indice)
	*(n)a == guarda o noh a do (novo) elemento
	*(n)b == guarda o noh b do (novo) elemento
	*(n)c/(n)c == analogo aos anteriores, soh que para as variaveis de controle do elemento (obs.: soh suporta uma variavel de controle)

*Funcoes:
	*fabs == absolute value of floating-point (read: man fabs)

*Obs.:
	TOLG -----> definiu que todos os valores menores que 10e-9 sao 0
	tipo 'O' -> operacional

*Traducao de linhas de mna1/info import:
	(linha 137)
		if (!(achou=!strcmp(nome,lista[i]))) i++;
		/*achou recebe o output de strcmp, se igual a zero nao entra no if
		se nao, incrementa o indice*/
	(linha 167) explicacao para o fato de nao utilizar a posicao 1 da matriz
	de analise nodal
	
