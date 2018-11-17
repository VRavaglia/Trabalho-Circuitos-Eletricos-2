///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Feb 17 2007)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#ifndef __GUIFrame__
#define __GUIFrame__
//#define __GXX_ABI_VERSION 1010

// Define WX_GCH in order to support precompiled headers with GCC compiler.
// You have to create the header "wx_pch.h" and include all files needed
// for compile your gui inside it.
// Then, compile it and place the file "wx_pch.h.gch" into the same
// directory that "wx_pch.h".
#ifdef WX_GCH
#include <wx_pch.h>
#else
#include <wx/wx.h>
#endif

#include <wx/menu.h>
#include <wx/richtext/richtextctrl.h>


///////////////////////////////////////////////////////////////////////////

#define idMenuQuit 1000
#define idMenuAbout 1001
#define idMenuOpen 1002
#define idMenuCalc 1003
#define idMenuCalcN 1004

///////////////////////////////////////////////////////////////////////////////
/// Class GUIFrame
///////////////////////////////////////////////////////////////////////////////
class GUIFrame : public wxFrame
{
    DECLARE_EVENT_TABLE()
    private:

        // Private event handlers
        void _wxFB_OnClose( wxCloseEvent& event ){ OnClose( event ); }
        void _wxFB_OnQuit( wxCommandEvent& event ){ OnQuit( event ); }
        void _wxFB_OnAbout( wxCommandEvent& event ){ OnAbout( event ); }
        void _wxFB_OnOpen( wxCommandEvent& event ){ OnOpen( event ); }
        void _wxFB_OnCalc( wxCommandEvent& event ){ OnCalc( event ); }
        void _wxFB_OnCalcN( wxCommandEvent& event ){ OnCalcN( event ); }


    protected:
        wxMenuBar* mbar;
        wxStatusBar* statusBar;

        // Virtual event handlers, overide them in your derived class
        virtual void OnClose( wxCloseEvent& event ){ event.Skip(); }
        virtual void OnQuit( wxCommandEvent& event ){ event.Skip(); }
        virtual void OnAbout( wxCommandEvent& event ){ event.Skip(); }
        virtual void OnOpen( wxCommandEvent& event ){ event.Skip(); }
        virtual void OnCalc( wxCommandEvent& event ){ event.Skip(); }
        virtual void OnCalcN( wxCommandEvent& event ){ event.Skip(); }


    public:
        GUIFrame( wxWindow* parent, int id = wxID_ANY, wxString title = wxT("Simulador"), wxPoint pos = wxDefaultPosition, wxSize size = wxSize( 800,500 ), int style = wxDEFAULT_FRAME_STYLE|wxTAB_TRAVERSAL );
        wxRichTextCtrl* console;
};

#endif //__GUIFrame__
