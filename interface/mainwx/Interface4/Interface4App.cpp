/***************************************************************
 * Name:      Interface4App.cpp
 * Purpose:   Code for Application Class
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

#include "Interface4App.h"
#include "Interface4Main.h"
#define __GXX_ABI_VERSION 1010
IMPLEMENT_APP(Interface4App);

bool Interface4App::OnInit()
{
    Interface4Frame* frame = new Interface4Frame(0L);
    frame->SetIcon(wxICON(aaaa)); // To Set App Icon
    frame->Show();

    return true;
}
