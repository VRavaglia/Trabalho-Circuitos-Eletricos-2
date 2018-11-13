///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Feb 17 2007)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif //__BORLANDC__

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif //WX_PRECOMP

#include "GUIFrame.h"
#include <wx/richtext/richtextctrl.h>
#define __GXX_ABI_VERSION 1010

///////////////////////////////////////////////////////////////////////////
BEGIN_EVENT_TABLE( GUIFrame, wxFrame )
    EVT_CLOSE( GUIFrame::_wxFB_OnClose )
    EVT_MENU( idMenuQuit, GUIFrame::_wxFB_OnQuit )
    EVT_MENU( idMenuAbout, GUIFrame::_wxFB_OnAbout )
    EVT_MENU( idMenuOpen, GUIFrame::_wxFB_OnOpen )
    EVT_MENU( idMenuCalc, GUIFrame::_wxFB_OnCalc )
END_EVENT_TABLE()

wxRichTextCtrl* console;

GUIFrame::GUIFrame( wxWindow* parent, int id, wxString title, wxPoint pos, wxSize size, int style ) : wxFrame( parent, id, title, pos, size, style )
{
    this->SetSizeHints( wxDefaultSize, wxDefaultSize );

    mbar = new wxMenuBar( 0 );
    wxMenu* fileMenu;
    fileMenu = new wxMenu();

    wxMenuItem* menuFileOpen = new wxMenuItem( fileMenu, idMenuOpen, wxString( wxT("&Abrir") ) + wxT('\t') + wxT("Ctrl+o"), wxT("Abrir arquivo de simulacao"), wxITEM_NORMAL );
    fileMenu->Append( menuFileOpen );

    wxMenuItem* menuFileQuit = new wxMenuItem( fileMenu, idMenuQuit, wxString( wxT("&Sair") ) + wxT('\t') + wxT("Alt+F4"), wxT("Sair da aplicacao"), wxITEM_NORMAL );
    fileMenu->Append( menuFileQuit );

    mbar->Append( fileMenu, wxT("&Arquivo") );

    wxMenu* helpMenu;

    helpMenu = new wxMenu();
    wxMenuItem* menuHelpAbout = new wxMenuItem( helpMenu, idMenuAbout, wxString( wxT("&Sobre") ) + wxT('\t') + wxT("F1"), wxT("Mostrar informacoes sobre a aplicacao"), wxITEM_NORMAL );
    helpMenu->Append( menuHelpAbout );
    mbar->Append( helpMenu, wxT("&Ajuda") );

    wxMenu* simulMenu;

    simulMenu = new wxMenu();
    wxMenuItem* menuCalcular = new wxMenuItem( simulMenu, idMenuCalc, wxString( wxT("&Calcular") ) + wxT('\t') + wxT("F9"), wxT("Calcula as tensoes e correntes no circuito fornecido"), wxITEM_NORMAL );
    simulMenu->Append( menuCalcular );

    mbar->Append( simulMenu, wxT("&Simular") );

    wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer( wxVERTICAL );

	console = new wxRichTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0|wxVSCROLL|wxHSCROLL|wxNO_BORDER|wxWANTS_CHARS );
	console->SetEditable(false);
	bSizer1->Add( console, 1, wxEXPAND | wxALL, 5 );


	this->SetSizer( bSizer1 );



    this->SetMenuBar( mbar );

    statusBar = this->CreateStatusBar( 2, wxST_SIZEGRIP, wxID_ANY );
}
