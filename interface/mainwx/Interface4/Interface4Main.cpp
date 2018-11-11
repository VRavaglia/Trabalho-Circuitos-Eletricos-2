/***************************************************************
 * Name:      Interface4Main.cpp
 * Purpose:   Code for Application Frame
 * Author:     ()
 * Created:   2018-11-11
 * Copyright:  ()
 * License:
 **************************************************************/

#ifdef WX_PRECOMP
#include "wx_pch.h"
#endif

#ifdef __BORLANDC__
#pragma hdrstop
#endif //__BORLANDC__

#include "Interface4Main.h"
#include "mnaFGR.h"
#define __GXX_ABI_VERSION 1010

//helper functions
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

Interface4Frame::Interface4Frame(wxFrame *frame)
    : GUIFrame(frame)
{
#if wxUSE_STATUSBAR
    statusBar->SetStatusText(_("Bem Vindo!"), 0);
    statusBar->SetStatusText(wxbuildinfo(short_f), 1);
#endif
}

Interface4Frame::~Interface4Frame()
{
}

void Interface4Frame::OnClose(wxCloseEvent &event)
{
    Destroy();
}

void Interface4Frame::OnQuit(wxCommandEvent &event)
{
    Destroy();
}

void Interface4Frame::OnAbout(wxCommandEvent &event)
{
    wxMessageBox(_("Simulador de circuitos feito para a disciplina de Circuitos Eletricos 2, UFRJ, 2018.2"), _("Simulador"));
}

void Interface4Frame::OnOpen(wxCommandEvent &event)
{

    wxFileDialog  fdlog(this);

    // show file dialog and get the path to
    // the file that was selected.
    if(fdlog.ShowModal() != wxID_OK) return;
    file.Clear();
    file = fdlog.GetPath();

    wxMessageBox(file, _("Simulador"));
}

void Interface4Frame::OnCalc(wxCommandEvent &event)
{

}
