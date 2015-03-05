/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  either of the following licenses
 *
 *         - GNU Lesser General Public License Version 2.1
 *         - Sun Industry Standards Source License Version 1.1
 *
 *  Sun Microsystems Inc., October, 2000
 *
 *  GNU Lesser General Public License Version 2.1
 *  =============================================
 *  Copyright 2000 by Sun Microsystems, Inc.
 *  901 San Antonio Road, Palo Alto, CA 94303, USA
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License version 2.1, as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *  MA  02111-1307  USA
 *
 *
 *  Sun Industry Standards Source License Version 1.1
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.1 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://www.openoffice.org/license.html.
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *  The Initial Developer of the Original Code is: IBM Corporation
 *
 *  Copyright: 2008 by IBM Corporation
 *
 *  All Rights Reserved.
 *
 *  Contributor(s): _______________________________________
 *
 *
 ************************************************************************/
/*************************************************************************
 * @file
 * Foot note object.
 ************************************************************************/
#ifndef INCLUDED_LOTUSWORDPRO_SOURCE_FILTER_XFILTER_XFFOOTNOTE_HXX
#define INCLUDED_LOTUSWORDPRO_SOURCE_FILTER_XFILTER_XFFOOTNOTE_HXX

#include "xfglobal.hxx"
#include "xfcontent.hxx"
#include "xfcontentcontainer.hxx"

/**
 * @brief
 * Foot note object.
 * Foot note has a lable, which is used in page content, and footnote contents in the footnote section.
 */
class XFFootNote : public XFContentContainer
{
public:
    XFFootNote();

public:
    void    SetLabel(sal_Unicode label);

    virtual void    ToXml(IXFStream *pStrm) SAL_OVERRIDE;

private:
    OUString   m_strID;
    OUString   m_strLabel;
};

inline XFFootNote::XFFootNote()
{
    m_strID = XFGlobal::GenNoteName();
}

inline void XFFootNote::SetLabel(sal_Unicode label)
{
    sal_Unicode     chs[2];
    chs[0] = label;
    chs[1] = 0;

    m_strLabel = OUString(chs);
}

inline void XFFootNote::ToXml(IXFStream *pStrm)
{
    IXFAttrList *pAttrList = pStrm->GetAttrList();
    pAttrList->Clear();

    pAttrList->AddAttribute( "text:id", m_strID );
    pStrm->StartElement( "text:footnote" );

    pAttrList->Clear();
    if( !m_strLabel.isEmpty() )
        pAttrList->AddAttribute( "text:label", m_strLabel );
    pStrm->StartElement( "text:footnote-citation" );
    if( !m_strLabel.isEmpty() )
        pStrm->Characters(m_strLabel);
    pStrm->EndElement( "text:footnote-citation" );

    pAttrList->Clear();
    pStrm->StartElement( "text:footnote-body" );
    XFContentContainer::ToXml(pStrm);
    pStrm->EndElement( "text:footnote-body" );

    pStrm->EndElement( "text:footnote" );
}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */