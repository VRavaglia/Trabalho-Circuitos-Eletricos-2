/***************************************************************
 * Name:      Interface4Main.h
 * Purpose:   Defines Application Frame
 * Author:     ()
 * Created:   2018-11-11
 * Copyright:  ()
 * License:
 **************************************************************/

#ifndef INTERFACE4MAIN_H
#define INTERFACE4MAIN_H
//#define __GXX_ABI_VERSION 1010



#include "Interface4App.h"


#include "GUIFrame.h"

class Interface4Frame: public GUIFrame
{
    public:
        wxString nome_arquivo;
        double tempo_inicial;
        double tempo_final;
        double delta_t;
        int p_op_ou_c_ini;

        Interface4Frame(wxFrame *frame);
        ~Interface4Frame();
        void i_erro(const char* format, ...);
        void i_printf(const char* , ...);
        wxString i_abrir();
    private:
        virtual void OnClose(wxCloseEvent& event);
        virtual void OnQuit(wxCommandEvent& event);
        virtual void OnAbout(wxCommandEvent& event);
        virtual void OnOpen(wxCommandEvent& event);
        virtual void OnCalc(wxCommandEvent& event);
        virtual void OnCalcN(wxCommandEvent& event);
};

#endif // INTERFACE4MAIN_H
