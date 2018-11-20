#ifndef MNAFGR_H_INCLUDED
#define MNAFGR_H_INCLUDED

class Interface4Frame;

int calculo(const char *caminho_netlist, Interface4Frame &frame, double i_tFinal, double i_deltaT, bool i_pontoOp);

#endif // MNAFGR_H_INCLUDED
