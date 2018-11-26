import sys
arquivo = sys.argv[1] #nome do arquivo

file = open(arquivo,'r')
data = dict()
chaves = []

for indice in file:
    string = indice.split(" ") #separa a string em 2 partes usando o separador ' '
    try:
        for elementos in range(len(string)):
             data[chaves[elementos]].append(float(string[elementos]))
    except:
        for elementos in range(len(string)):
            chaves.append(string[elementos])
            data.update({string[elementos]:[]})
            
import matplotlib
import matplotlib.pyplot as plt

primeiraChave = 0 # serve para salvar a chave tempo
chaveTempo = ""
valorTempo = 0
for key, value in data.items():
    if (primeiraChave == 0):
        chave = key #salva o nome da primeira chave
        valorTempo = value # salva o valor da primeira chave ou seja, os tempos
    else:
        break
    primeiraChave = primeiraChave + 1

interact = 0 #usado para gerar o nome dos arquivos por interação
for key, value in data.items():
    if(len(value) == 1):
        break
    else:
        fig, ax = plt.subplots(figsize=(12,8))
        ax.plot(valorTempo, value)
        ax.set(xlabel='time (s)', ylabel= key)
        ax.grid()
        fig.savefig(key + ".png", dpi = 300)
        interact += 1
