
#ifdef WX_PRECOMP
#include "wx_pch.h"
#endif

#ifdef __BORLANDC__
#pragma hdrstop
#endif //__BORLANDC__

#include "Interface4App.h"
#include "Interface4Main.h"
#define __GXX_ABI_VERSION 1010
IMPLEMENT_APP(Interface4App);

bool Interface4App::OnInit()
{
    //Cria e mostra a interface
    Interface4Frame* frame = new Interface4Frame(0L);
    frame->SetIcon(wxICON(aaaa));
    frame->Show();

    return true;
}
