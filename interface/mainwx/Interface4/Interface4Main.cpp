#ifdef WX_PRECOMP
#include "wx_pch.h"
#endif

#ifdef __BORLANDC__
#pragma hdrstop
#endif //__BORLANDC__

#include "Interface4Main.h"
#include "mnaFGR.h"
#include <wx/textdlg.h>
#define __GXX_ABI_VERSION 1010

//Conversao do MNA1
//O programa original foi projetado para ter como interface de entrada e saida o console
//por isso foi necessario criar funcoes que adaptassem as funcoes antigas para a nova interface


//Toda fez que um erro for encontrado, em vez de uma mensagem no console, e exibida
//uma janela informando qual o erro ocorrido

void Interface4Frame::i_erro(const char* format, ...){
    //Necessario para que a funcao tenha o mesmo formato de entrada da funcao original
    //,ou seja , recebe strings no padrao que a funcao printf() recebe
    char buffer[1024];
    va_list argptr;
    va_start(argptr, format);
    vsnprintf(buffer, 1024, format, argptr);
    va_end(argptr);

    //A janela e criada com as informacoes sobre oe rro
    wxString msg_erro = wxString::FromUTF8(buffer);
    wxMessageBox(msg_erro, _("O seguinte erro ocorreu:"));
};


//Funcao que escreve os resultados da simulacao no console da interface
void Interface4Frame::i_printf(const char* format, ...){
    //Novamente a conversao do formato printf()
    char buffer[1024];
    va_list argptr;
    va_start(argptr, format);
    vsnprintf(buffer, 1024, format, argptr);
    va_end(argptr);

    //Anexa a informacao no console
    wxString msg = wxString::FromUTF8(buffer);
    console->AppendText(msg);

};

//Exibe o dialogo para que o arquivo fonte possa ser selecionado
wxString Interface4Frame::i_abrir(){
    //Exibe o dialogo
    wxFileDialog  fdlog(this, _("Selecione a netlist"), _(""), _(""), _("Arquivos netlist (*.net)|*.net"));
    wxString file;

    //Caso um arquivo tenha sido selecionado retorna o caminho para o arquivo
    if(fdlog.ShowModal() != wxID_OK) return _("");
    file.Clear();
    file = fdlog.GetPath();

    return file;
};


//Fim conversao


//Funcoes que sao cridas automaticamente, estao relacionadas com
//o funcionamento da multi-plataforma
enum wxbuildinfoformat {
    short_f, long_f };



wxString wxbuildinfo(wxbuildinfoformat format)
{
    wxString wxbuild(wxVERSION_STRING);

    if (format == long_f )
    {
#if defined(__WXMSW__)
        wxbuild << _T("-Windows");
#elif defined(__WXMAC__)
        wxbuild << _T("-Mac");
#elif defined(__UNIX__)
        wxbuild << _T("-Linux");
#endif

#if wxUSE_UNICODE
        wxbuild << _T("-Unicode build");
#else
        wxbuild << _T("-ANSI build");
#endif // wxUSE_UNICODE
    }

    return wxbuild;
}


wxString file;

//Interface em si
Interface4Frame::Interface4Frame(wxFrame *frame)
    : GUIFrame(frame)
{
#if wxUSE_STATUSBAR
    statusBar->SetStatusText(_("Bem Vindo!"), 0);
    statusBar->SetStatusText(wxbuildinfo(short_f), 1);
#endif
}

//Funcoes que sao chamadas para um determinado evento

Interface4Frame::~Interface4Frame()
{
}

//Ao clicar no botao de fechar a interface, destruir a interface (GUI)
void Interface4Frame::OnClose(wxCloseEvent &event)
{
    Destroy();
}

//Ao clicar no botao de sair, destruir a interface (GUI)
void Interface4Frame::OnQuit(wxCommandEvent &event)
{
    Destroy();
}

//Ao clicar no botao "sobre", exibi uma breve descricao do programa
void Interface4Frame::OnAbout(wxCommandEvent &event)
{
    wxMessageBox(_("Simulador de circuitos feito para a disciplina de Circuitos Eletricos 2, UFRJ, 2018.2"), _("Simulador"));
}

//Ao clicar no botao "abrir" exibe o dialogo dde slecao de arquivo
void Interface4Frame::OnOpen(wxCommandEvent &event)
{

    nome_arquivo = this->i_abrir();
}

//Ao clicar no botao "calcular" inicia-se o processo de simulacao
void Interface4Frame::OnCalc(wxCommandEvent &event)
{
    //Se o arquivo ainda nao foi especificado, cria um dialogo para que ele posssa ser selecionado
    if (nome_arquivo.empty()){
        nome_arquivo = this->i_abrir();
    }



    //Limpa o console e exibe uma lista dos parametros escolhidos
    console->Clear();

    //Inicia a simulacao de fato chamando a funcao principal contida no MNA1 modificado
    calculo(nome_arquivo.mb_str(), *this);
}
