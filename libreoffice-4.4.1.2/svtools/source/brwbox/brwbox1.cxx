/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <svtools/brwbox.hxx>
#include <svtools/brwhead.hxx>
#include <o3tl/numeric.hxx>
#include "datwin.hxx"
#include <tools/debug.hxx>
#include <tools/stream.hxx>
#include <tools/fract.hxx>

#include <functional>
#include <algorithm>
#include <com/sun/star/accessibility/AccessibleTableModelChange.hpp>
#include <com/sun/star/accessibility/AccessibleTableModelChangeType.hpp>
#include <com/sun/star/accessibility/AccessibleEventId.hpp>
#include <com/sun/star/accessibility/XAccessible.hpp>
#include <tools/multisel.hxx>
#include "brwimpl.hxx"


#define SCROLL_FLAGS (SCROLL_CLIP | SCROLL_NOCHILDREN)
#define getDataWindow() (static_cast<BrowserDataWin*>(pDataWin))

using namespace com::sun::star::accessibility::AccessibleEventId;
using namespace com::sun::star::accessibility::AccessibleTableModelChangeType;
using com::sun::star::accessibility::AccessibleTableModelChange;
using com::sun::star::lang::XComponent;
using namespace ::com::sun::star::uno;
using namespace svt;

namespace
{
    void disposeAndClearHeaderCell(::svt::BrowseBoxImpl::THeaderCellMap& _rHeaderCell)
    {
        ::std::for_each(
                        _rHeaderCell.begin(),
                        _rHeaderCell.end(),
                        ::svt::BrowseBoxImpl::THeaderCellMapFunctorDispose()
                            );
        _rHeaderCell.clear();
    }
}



void BrowseBox::ConstructImpl( BrowserMode nMode )
{
    OSL_TRACE( "BrowseBox: %p->ConstructImpl", this );
    bMultiSelection = false;
    pColSel = 0;
    pDataWin = 0;
    pVScroll = 0;

    pDataWin = new BrowserDataWin( this );
    pCols = new BrowserColumns;
    m_pImpl.reset( new ::svt::BrowseBoxImpl() );

    aGridLineColor = Color( COL_LIGHTGRAY );
    InitSettings_Impl( this );
    InitSettings_Impl( pDataWin );

    bBootstrapped = false;
    nDataRowHeight = 0;
    nTitleLines = 1;
    nFirstCol = 0;
    nTopRow = 0;
    nCurRow = BROWSER_ENDOFSELECTION;
    nCurColId = 0;
    bResizing = false;
    bSelect = false;
    bSelecting = false;
    bScrolling = false;
    bSelectionIsVisible = false;
    bNotToggleSel = false;
    bRowDividerDrag = false;
    bHit = false;
    mbInteractiveRowHeight = false;
    bHideSelect = false;
    bHideCursor = TRISTATE_FALSE;
    nRowCount = 0;
    m_bFocusOnlyCursor = true;
    m_aCursorColor = COL_TRANSPARENT;
    m_nCurrentMode = 0;
    nControlAreaWidth = USHRT_MAX;
    uRow.nSel = BROWSER_ENDOFSELECTION;

    aHScroll.SetLineSize(1);
    aHScroll.SetScrollHdl( LINK( this, BrowseBox, ScrollHdl ) );
    aHScroll.SetEndScrollHdl( LINK( this, BrowseBox, EndScrollHdl ) );
    pDataWin->Show();

    SetMode( nMode );
    bSelectionIsVisible = bKeepHighlight;
    bHasFocus = HasChildPathFocus();
    getDataWindow()->nCursorHidden =
                ( bHasFocus ? 0 : 1 ) + ( GetUpdateMode() ? 0 : 1 );
}



BrowseBox::BrowseBox( vcl::Window* pParent, WinBits nBits, BrowserMode nMode )
    :Control( pParent, nBits | WB_3DLOOK )
    ,DragSourceHelper( this )
    ,DropTargetHelper( this )
    ,aHScroll( this, WinBits( WB_HSCROLL ) )
{
    ConstructImpl( nMode );
}



BrowseBox::BrowseBox( vcl::Window* pParent, const ResId& rId, BrowserMode nMode )
    :Control( pParent, rId )
    ,DragSourceHelper( this )
    ,DropTargetHelper( this )
    ,aHScroll( this, WinBits(WB_HSCROLL) )
{
    ConstructImpl(nMode);
}


BrowseBox::~BrowseBox()
{
    OSL_TRACE( "BrowseBox: %p~", this );

    if ( m_pImpl->m_pAccessible )
    {
        disposeAndClearHeaderCell(m_pImpl->m_aColHeaderCellMap);
        disposeAndClearHeaderCell(m_pImpl->m_aRowHeaderCellMap);
        m_pImpl->m_pAccessible->dispose();
    }

    Hide();
    delete getDataWindow()->pHeaderBar;
    delete getDataWindow()->pCornerWin;
    delete pDataWin;
    delete pVScroll;

    // free columns-space
    for ( size_t i = 0, n = pCols->size(); i < n; ++i )
        delete (*pCols)[ i ];
    pCols->clear();
    delete pCols;
    delete pColSel;
    if ( bMultiSelection )
        delete uRow.pSel;
}



short BrowseBox::GetCursorHideCount() const
{
    return getDataWindow()->nCursorHidden;
}



void BrowseBox::DoShowCursor( const char * )
{
    short nHiddenCount = --getDataWindow()->nCursorHidden;
    if (PaintCursorIfHiddenOnce())
    {
        if (1 == nHiddenCount)
            DrawCursor();
    }
    else
    {
        if (0 == nHiddenCount)
            DrawCursor();
    }
}



void BrowseBox::DoHideCursor( const char * )
{
    short nHiddenCount = ++getDataWindow()->nCursorHidden;
    if (PaintCursorIfHiddenOnce())
    {
        if (2 == nHiddenCount)
            DrawCursor();
    }
    else
    {
        if (1 == nHiddenCount)
            DrawCursor();
    }
}



void BrowseBox::SetRealRowCount( const OUString &rRealRowCount )
{
    getDataWindow()->aRealRowCount = rRealRowCount;
}



void BrowseBox::SetFont( const vcl::Font& rNewFont )
{
    pDataWin->SetFont( rNewFont );
    ImpGetDataRowHeight();
}



sal_uLong BrowseBox::GetDefaultColumnWidth( const OUString& _rText ) const
{
    return GetDataWindow().GetTextWidth( _rText ) + GetDataWindow().GetTextWidth(OUString('0')) * 4;
}



void BrowseBox::InsertHandleColumn( sal_uLong nWidth )
{

#if OSL_DEBUG_LEVEL > 0
    OSL_ENSURE( ColCount() == 0 || (*pCols)[0]->GetId() != HandleColumnId , "BrowseBox::InsertHandleColumn: there is already a handle column" );
    {
        BrowserColumns::iterator iCol = pCols->begin();
        const BrowserColumns::iterator colsEnd = pCols->end();
        if ( iCol < colsEnd )
            for (++iCol; iCol < colsEnd; ++iCol)
                OSL_ENSURE( (*iCol)->GetId() != HandleColumnId, "BrowseBox::InsertHandleColumn: there is a non-Handle column with handle ID" );
    }
#endif

    pCols->insert( pCols->begin(), new BrowserColumn( 0, Image(), OUString(), nWidth, GetZoom() ) );
    FreezeColumn( 0 );

    // adjust headerbar
    if ( getDataWindow()->pHeaderBar )
    {
        getDataWindow()->pHeaderBar->SetPosSizePixel(
                    Point(nWidth, 0),
                    Size( GetOutputSizePixel().Width() - nWidth, GetTitleHeight() )
                    );
    }

    ColumnInserted( 0 );
}



void BrowseBox::InsertDataColumn( sal_uInt16 nItemId, const OUString& rText,
        long nWidth, HeaderBarItemBits nBits, sal_uInt16 nPos )
{

    OSL_ENSURE( nItemId != HandleColumnId, "BrowseBox::InsertDataColumn: nItemId is HandleColumnId" );
    OSL_ENSURE( nItemId != BROWSER_INVALIDID, "BrowseBox::InsertDataColumn: nItemId is reserved value BROWSER_INVALIDID" );

#if OSL_DEBUG_LEVEL > 0
    {
        const BrowserColumns::iterator colsEnd = pCols->end();
        for (BrowserColumns::iterator iCol = pCols->begin(); iCol < colsEnd; ++iCol)
            OSL_ENSURE( (*iCol)->GetId() != nItemId, "BrowseBox::InsertDataColumn: duplicate column Id" );
    }
#endif

    if ( nPos < pCols->size() )
    {
        BrowserColumns::iterator it = pCols->begin();
        ::std::advance( it, nPos );
        pCols->insert( it, new BrowserColumn( nItemId, Image(), rText, nWidth, GetZoom() ) );
    }
    else
    {
        pCols->push_back( new BrowserColumn( nItemId, Image(), rText, nWidth, GetZoom() ) );
    }
    if ( nCurColId == 0 )
        nCurColId = nItemId;

    if ( getDataWindow()->pHeaderBar )
    {
        // Handle column not in the header bar
        sal_uInt16 nHeaderPos = nPos;
        if (nHeaderPos != HEADERBAR_APPEND && GetColumnId(0) == HandleColumnId )
            nHeaderPos--;
        getDataWindow()->pHeaderBar->InsertItem(
                nItemId, rText, nWidth, nBits, nHeaderPos );
    }
    ColumnInserted( nPos );
}


sal_uInt16 BrowseBox::ToggleSelectedColumn()
{
    sal_uInt16 nSelectedColId = BROWSER_INVALIDID;
    if ( pColSel && pColSel->GetSelectCount() )
    {
        DoHideCursor( "ToggleSelectedColumn" );
        ToggleSelection();
        nSelectedColId = (*pCols)[ pColSel->FirstSelected() ]->GetId();
        pColSel->SelectAll(false);
    }
    return nSelectedColId;
}

void BrowseBox::SetToggledSelectedColumn(sal_uInt16 _nSelectedColumnId)
{
    if ( pColSel && _nSelectedColumnId != BROWSER_INVALIDID )
    {
        pColSel->Select( GetColumnPos( _nSelectedColumnId ) );
        ToggleSelection();
        OSL_TRACE( "BrowseBox: %p->SetToggledSelectedColumn", this );
        DoShowCursor( "SetToggledSelectedColumn" );
    }
}

void BrowseBox::FreezeColumn( sal_uInt16 nItemId, bool bFreeze )
{

    // never unfreeze the handle-column
    if ( nItemId == HandleColumnId && !bFreeze )
        return;

    // get the position in the current array
    size_t nItemPos = GetColumnPos( nItemId );
    if ( nItemPos >= pCols->size() )
        // not available!
        return;

    // doesn't the state change?
    if ( (*pCols)[ nItemPos ]->IsFrozen() == bFreeze )
        return;

    // remark the column selection
    sal_uInt16 nSelectedColId = ToggleSelectedColumn();

    // freeze or unfreeze?
    if ( bFreeze )
    {
        // to be moved?
        if ( nItemPos != 0 && !(*pCols)[ nItemPos-1 ]->IsFrozen() )
        {
            // move to the right of the last frozen column
            sal_uInt16 nFirstScrollable = FrozenColCount();
            BrowserColumn *pColumn = (*pCols)[ nItemPos ];
            BrowserColumns::iterator it = pCols->begin();
            ::std::advance( it, nItemPos );
            pCols->erase( it );
            nItemPos = nFirstScrollable;
            it = pCols->begin();
            ::std::advance( it, nItemPos );
            pCols->insert( it, pColumn );
        }

        // adjust the number of the first scrollable and visible column
        if ( nFirstCol <= nItemPos )
            nFirstCol = nItemPos + 1;
    }
    else
    {
        // to be moved?
        if ( (sal_Int32)nItemPos != FrozenColCount()-1 )
        {
            // move to the leftmost scrollable column
            sal_uInt16 nFirstScrollable = FrozenColCount();
            BrowserColumn *pColumn = (*pCols)[ nItemPos ];
            BrowserColumns::iterator it = pCols->begin();
            ::std::advance( it, nItemPos );
            pCols->erase( it );
            nItemPos = nFirstScrollable;
            it = pCols->begin();
            ::std::advance( it, nItemPos );
            pCols->insert( it, pColumn );
        }

        // adjust the number of the first scrollable and visible column
        nFirstCol = nItemPos;
    }

    // toggle the freeze-state of the column
    (*pCols)[ nItemPos ]->Freeze( bFreeze );

    // align the scrollbar-range
    UpdateScrollbars();

    // repaint
    Control::Invalidate();
    getDataWindow()->Invalidate();

    // remember the column selection
    SetToggledSelectedColumn(nSelectedColId);
}



void BrowseBox::SetColumnPos( sal_uInt16 nColumnId, sal_uInt16 nPos )
{
    // never set pos of the handle column
    if ( nColumnId == HandleColumnId )
        return;

    // get the position in the current array
    sal_uInt16 nOldPos = GetColumnPos( nColumnId );
    if ( nOldPos >= pCols->size() )
        // not available!
        return;

    // does the state change?
    if (nOldPos != nPos)
    {
        // remark the column selection
        sal_uInt16 nSelectedColId = ToggleSelectedColumn();

        // determine old column area
        Size aDataWinSize( pDataWin->GetSizePixel() );
        if ( getDataWindow()->pHeaderBar )
            aDataWinSize.Height() += getDataWindow()->pHeaderBar->GetSizePixel().Height();

        Rectangle aFromRect( GetFieldRect( nColumnId) );
        aFromRect.Right() += 2*MIN_COLUMNWIDTH;

        sal_uInt16 nNextPos = nOldPos + 1;
        if ( nOldPos > nPos )
            nNextPos = nOldPos - 1;

        BrowserColumn *pNextCol = (*pCols)[ nNextPos ];
        Rectangle aNextRect(GetFieldRect( pNextCol->GetId() ));

        // move column internally
        {
            BrowserColumns::iterator it = pCols->begin();
            ::std::advance( it, nOldPos );
            BrowserColumn* pTemp = *it;
            pCols->erase( it );
            it = pCols->begin();
            ::std::advance( it, nPos );
            pCols->insert( it, pTemp );
        }

        // determine new column area
        Rectangle aToRect( GetFieldRect( nColumnId ) );
        aToRect.Right() += 2*MIN_COLUMNWIDTH;

        // do scroll, let redraw
        if( pDataWin->GetBackground().IsScrollable() )
        {
            long nScroll = -aFromRect.GetWidth();
            Rectangle aScrollArea;
            if ( nOldPos > nPos )
            {
                long nFrozenWidth = GetFrozenWidth();
                if ( aToRect.Left() < nFrozenWidth )
                    aToRect.Left() = nFrozenWidth;
                aScrollArea = Rectangle(Point(aToRect.Left(),0),
                                        Point(aNextRect.Right(),aDataWinSize.Height()));
                nScroll *= -1; // reverse direction
            }
            else
                aScrollArea = Rectangle(Point(aNextRect.Left(),0),
                                        Point(aToRect.Right(),aDataWinSize.Height()));

            pDataWin->Scroll( nScroll, 0, aScrollArea );
            aToRect.Top() = 0;
            aToRect.Bottom() = aScrollArea.Bottom();
            Invalidate( aToRect );
        }
        else
            pDataWin->Window::Invalidate( INVALIDATE_NOCHILDREN );

        // adjust header bar positions
        if ( getDataWindow()->pHeaderBar )
        {
            sal_uInt16 nNewPos = nPos;
            if ( GetColumnId(0) == HandleColumnId )
                --nNewPos;
            getDataWindow()->pHeaderBar->MoveItem(nColumnId,nNewPos);
        }
        // remember the column selection
        SetToggledSelectedColumn(nSelectedColId);

        if ( isAccessibleAlive() )
        {
            commitTableEvent(
                TABLE_MODEL_CHANGED,
                makeAny( AccessibleTableModelChange(
                            DELETE,
                            0,
                            GetRowCount(),
                            nOldPos,
                            nOldPos
                        )
                ),
                Any()
            );

            commitTableEvent(
                TABLE_MODEL_CHANGED,
                makeAny( AccessibleTableModelChange(
                            INSERT,
                            0,
                            GetRowCount(),
                            nPos,
                            nPos
                        )
                ),
                Any()
            );
        }
    }

}



void BrowseBox::SetColumnTitle( sal_uInt16 nItemId, const OUString& rTitle )
{

    // never set title of the handle-column
    if ( nItemId == HandleColumnId )
        return;

    // get the position in the current array
    sal_uInt16 nItemPos = GetColumnPos( nItemId );
    if ( nItemPos >= pCols->size() )
        // not available!
        return;

    // does the state change?
    BrowserColumn *pCol = (*pCols)[ nItemPos ];
    if ( pCol->Title() != rTitle )
    {
        OUString sNew(rTitle);
        OUString sOld(pCol->Title());

        pCol->Title() = rTitle;

        // adjust headerbar column
        if ( getDataWindow()->pHeaderBar )
            getDataWindow()->pHeaderBar->SetItemText( nItemId, rTitle );
        else
        {
            // redraw visible columns
            if ( GetUpdateMode() && ( pCol->IsFrozen() || nItemPos > nFirstCol ) )
                Invalidate( Rectangle( Point(0,0),
                    Size( GetOutputSizePixel().Width(), GetTitleHeight() ) ) );
        }

        if ( isAccessibleAlive() )
        {
            commitTableEvent(   TABLE_COLUMN_DESCRIPTION_CHANGED,
                makeAny( sNew ),
                makeAny( sOld )
            );
        }
    }
}



void BrowseBox::SetColumnWidth( sal_uInt16 nItemId, sal_uLong nWidth )
{

    // get the position in the current array
    size_t nItemPos = GetColumnPos( nItemId );
    if ( nItemPos >= pCols->size() )
        return;

    // does the state change?
    nWidth = QueryColumnResize( nItemId, nWidth );
    if ( nWidth >= LONG_MAX || (*pCols)[ nItemPos ]->Width() != nWidth )
    {
        long nOldWidth = (*pCols)[ nItemPos ]->Width();

        // adjust last column, if necessary
        if ( IsVisible() && nItemPos == pCols->size() - 1 )
        {
            long nMaxWidth = pDataWin->GetSizePixel().Width();
            nMaxWidth -= getDataWindow()->bAutoSizeLastCol
                    ? GetFieldRect(nItemId).Left()
                    : GetFrozenWidth();
            if ( static_cast<BrowserDataWin*>(pDataWin )->bAutoSizeLastCol || nWidth > (sal_uLong)nMaxWidth )
            {
                nWidth = nMaxWidth > 16 ? nMaxWidth : nOldWidth;
                nWidth = QueryColumnResize( nItemId, nWidth );
            }
        }

        // OV
        // In AutoSizeLastColumn(), we call SetColumnWidth with nWidth==0xffff.
        // Thus, check here, if the width has actually changed.
        if( (sal_uLong)nOldWidth == nWidth )
            return;

        // do we want to display the change immediately?
        bool bUpdate = GetUpdateMode() &&
                       ( (*pCols)[ nItemPos ]->IsFrozen() || nItemPos >= nFirstCol );

        if ( bUpdate )
        {
            // Selection hidden
            DoHideCursor( "SetColumnWidth" );
            ToggleSelection();
            //!getDataWindow()->Update();
            //!Control::Update();
        }

        // set width
        (*pCols)[ nItemPos ]->SetWidth(nWidth, GetZoom());

        // scroll and invalidate
        if ( bUpdate )
        {
            // get X-Pos of the column changed
            long nX = 0;
            for ( sal_uInt16 nCol = 0; nCol < nItemPos; ++nCol )
            {
                BrowserColumn *pCol = (*pCols)[ nCol ];
                if ( pCol->IsFrozen() || nCol >= nFirstCol )
                    nX += pCol->Width();
            }

            // actually scroll+invalidate
            pDataWin->SetClipRegion();
            bool bSelVis = bSelectionIsVisible;
            bSelectionIsVisible = false;
            if( GetBackground().IsScrollable() )
            {

                Rectangle aScrRect( nX + std::min( (sal_uLong)nOldWidth, nWidth ), 0,
                                    GetSizePixel().Width() , // the header is longer than the datawin
                                    pDataWin->GetPosPixel().Y() - 1 );
                Control::Scroll( nWidth-nOldWidth, 0, aScrRect, SCROLL_FLAGS );
                aScrRect.Bottom() = pDataWin->GetSizePixel().Height();
                getDataWindow()->Scroll( nWidth-nOldWidth, 0, aScrRect, SCROLL_FLAGS );
                Rectangle aInvRect( nX, 0, nX + std::max( nWidth, (sal_uLong)nOldWidth ), USHRT_MAX );
                Control::Invalidate( aInvRect, INVALIDATE_NOCHILDREN );
                static_cast<BrowserDataWin*>( pDataWin )->Invalidate( aInvRect );
            }
            else
            {
                Control::Invalidate( INVALIDATE_NOCHILDREN );
                getDataWindow()->Window::Invalidate( INVALIDATE_NOCHILDREN );
            }


            //!getDataWindow()->Update();
            //!Control::Update();
            bSelectionIsVisible = bSelVis;
            ToggleSelection();
            DoShowCursor( "SetColumnWidth" );
        }
        UpdateScrollbars();

        // adjust headerbar column
        if ( getDataWindow()->pHeaderBar )
            getDataWindow()->pHeaderBar->SetItemSize(
                    nItemId ? nItemId : USHRT_MAX - 1, nWidth );

        // adjust last column
        if ( nItemPos != pCols->size() - 1 )
            AutoSizeLastColumn();

    }
}



void BrowseBox::AutoSizeLastColumn()
{
    if ( getDataWindow()->bAutoSizeLastCol &&
         getDataWindow()->GetUpdateMode() )
    {
        sal_uInt16 nId = GetColumnId( (sal_uInt16)pCols->size() - 1 );
        SetColumnWidth( nId, LONG_MAX );
        ColumnResized( nId );
    }
}



void BrowseBox::RemoveColumn( sal_uInt16 nItemId )
{

    // get column position
    sal_uInt16 nPos = GetColumnPos(nItemId);
    if ( nPos >= ColCount() )
        // not available
        return;

    // correct column selection
    if ( pColSel )
        pColSel->Remove( nPos );

    // correct column cursor
    if ( nCurColId == nItemId )
        nCurColId = 0;

    // delete column
    BrowserColumns::iterator it = pCols->begin();
    ::std::advance( it, nPos );
    delete *it;
    pCols->erase( it );
    if ( nFirstCol >= nPos && nFirstCol > FrozenColCount() )
    {
        OSL_ENSURE(nFirstCol > 0,"FirstCol must be greater zero!");
        --nFirstCol;
    }

    // handlecolumn not in headerbar
    if (nItemId)
    {
        if ( getDataWindow()->pHeaderBar )
            getDataWindow()->pHeaderBar->RemoveItem( nItemId );
    }
    else
    {
        // adjust headerbar
        if ( getDataWindow()->pHeaderBar )
        {
            getDataWindow()->pHeaderBar->SetPosSizePixel(
                        Point(0, 0),
                        Size( GetOutputSizePixel().Width(), GetTitleHeight() )
                        );
        }
    }

    // correct vertical scrollbar
    UpdateScrollbars();

    // trigger repaint, if necessary
    if ( GetUpdateMode() )
    {
        getDataWindow()->Invalidate();
        Control::Invalidate();
        if ( getDataWindow()->bAutoSizeLastCol && nPos ==ColCount() )
            SetColumnWidth( GetColumnId( nPos - 1 ), LONG_MAX );
    }

    if ( isAccessibleAlive() )
    {
        commitTableEvent(
            TABLE_MODEL_CHANGED,
            makeAny( AccessibleTableModelChange(    DELETE,
                                                    0,
                                                    GetRowCount(),
                                                    nPos,
                                                    nPos
                                               )
            ),
            Any()
        );

        commitHeaderBarEvent(
            CHILD,
            Any(),
            makeAny( CreateAccessibleColumnHeader( nPos ) ),
            true
        );
    }
}



void BrowseBox::RemoveColumns()
{

    size_t nOldCount = pCols->size();

    // remove all columns
    for ( size_t i = 0; i < nOldCount; ++i )
        delete (*pCols)[ i ];
    pCols->clear();

    // correct column selection
    if ( pColSel )
    {
        pColSel->SelectAll(false);
        pColSel->SetTotalRange( Range( 0, 0 ) );
    }

    // correct column cursor
    nCurColId = 0;
    nFirstCol = 0;

    if ( getDataWindow()->pHeaderBar )
        getDataWindow()->pHeaderBar->Clear( );

    // correct vertical scrollbar
    UpdateScrollbars();

    // trigger repaint if necessary
    if ( GetUpdateMode() )
    {
        getDataWindow()->Invalidate();
        Control::Invalidate();
    }

    if ( isAccessibleAlive() )
    {
        if ( pCols->size() != nOldCount )
        {
            // all columns should be removed, so we remove the column header bar and append it again
            // to avoid to notify every column remove
            commitBrowseBoxEvent(
                CHILD,
                Any(),
                makeAny(m_pImpl->getAccessibleHeaderBar(BBTYPE_COLUMNHEADERBAR))
            );

            // and now append it again
            commitBrowseBoxEvent(
                CHILD,
                makeAny(m_pImpl->getAccessibleHeaderBar(BBTYPE_COLUMNHEADERBAR)),
                Any()
            );

            // notify a table model change
            commitTableEvent(
                TABLE_MODEL_CHANGED,
                makeAny ( AccessibleTableModelChange( DELETE,
                                0,
                                GetRowCount(),
                                0,
                                nOldCount
                            )
                        ),
                Any()
            );
        }
    }
}



OUString BrowseBox::GetColumnTitle( sal_uInt16 nId ) const
{

    sal_uInt16 nItemPos = GetColumnPos( nId );
    if ( nItemPos >= pCols->size() )
        return OUString();
    return (*pCols)[ nItemPos ]->Title();
}



long BrowseBox::GetRowCount() const
{
    return nRowCount;
}



sal_uInt16 BrowseBox::ColCount() const
{

    return (sal_uInt16) pCols->size();
}



long BrowseBox::ImpGetDataRowHeight() const
{

    BrowseBox *pThis = (BrowseBox*)this;
    pThis->nDataRowHeight = pThis->CalcReverseZoom(pDataWin->GetTextHeight() + 2);
    pThis->Resize();
    getDataWindow()->Invalidate();
    return nDataRowHeight;
}



void BrowseBox::SetDataRowHeight( long nPixel )
{

    nDataRowHeight = CalcReverseZoom(nPixel);
    Resize();
    getDataWindow()->Invalidate();
}



void BrowseBox::SetTitleLines( sal_uInt16 nLines )
{

    nTitleLines = nLines;
}



long BrowseBox::ScrollColumns( long nCols )
{

    if ( nFirstCol + nCols < 0 ||
         nFirstCol + nCols >= (long)pCols->size() )
        return 0;

    // implicitly hides cursor while scrolling
    StartScroll();
    bScrolling = true;
    bool bScrollable = pDataWin->GetBackground().IsScrollable();
    bool bInvalidateView = false;

    // scrolling one column to the right?
    if ( nCols == 1 )
    {
        // update internal value and scrollbar
        ++nFirstCol;
        aHScroll.SetThumbPos( nFirstCol - FrozenColCount() );

        if ( !bScrollable )
        {
            bInvalidateView = true;
        }
        else
        {
            long nDelta = (*pCols)[ nFirstCol-1 ]->Width();
            long nFrozenWidth = GetFrozenWidth();

            Rectangle aScrollRect(  Point( nFrozenWidth + nDelta, 0 ),
                                    Size ( GetOutputSizePixel().Width() - nFrozenWidth - nDelta,
                                           GetTitleHeight() - 1
                                         ) );

            // scroll the header bar area (if there is no dedicated HeaderBar control)
            if ( !getDataWindow()->pHeaderBar && nTitleLines )
            {
                // actually scroll
                Scroll( -nDelta, 0, aScrollRect, SCROLL_FLAGS );

                // invalidate the area of the column which was scrolled out to the left hand side
                Rectangle aInvalidateRect( aScrollRect );
                aInvalidateRect.Left() = nFrozenWidth;
                aInvalidateRect.Right() = nFrozenWidth + nDelta - 1;
                Invalidate( aInvalidateRect );
            }

            // scroll the data-area
            aScrollRect.Bottom() = pDataWin->GetOutputSizePixel().Height();

            // actually scroll
            pDataWin->Scroll( -nDelta, 0, aScrollRect, SCROLL_FLAGS );

            // invalidate the area of the column which was scrolled out to the left hand side
            aScrollRect.Left() = nFrozenWidth;
            aScrollRect.Right() = nFrozenWidth + nDelta - 1;
            getDataWindow()->Invalidate( aScrollRect );
        }
    }

    // scrolling one column to the left?
    else if ( nCols == -1 )
    {
        --nFirstCol;
        aHScroll.SetThumbPos( nFirstCol - FrozenColCount() );

        if ( !bScrollable )
        {
            bInvalidateView = true;
        }
        else
        {
            long nDelta = (*pCols)[ nFirstCol ]->Width();
            long nFrozenWidth = GetFrozenWidth();

            Rectangle aScrollRect(  Point(  nFrozenWidth, 0 ),
                                    Size (  GetOutputSizePixel().Width() - nFrozenWidth,
                                            GetTitleHeight() - 1
                                         ) );

            // scroll the header bar area (if there is no dedicated HeaderBar control)
            if ( !getDataWindow()->pHeaderBar && nTitleLines )
            {
                Scroll( nDelta, 0, aScrollRect, SCROLL_FLAGS );
            }

            // scroll the data-area
            aScrollRect.Bottom() = pDataWin->GetOutputSizePixel().Height();
            pDataWin->Scroll( nDelta, 0, aScrollRect, SCROLL_FLAGS );
        }
    }
    else
    {
        if ( GetUpdateMode() )
        {
            Invalidate( Rectangle(
                Point( GetFrozenWidth(), 0 ),
                Size( GetOutputSizePixel().Width(), GetTitleHeight() ) ) );
            getDataWindow()->Invalidate( Rectangle(
                Point( GetFrozenWidth(), 0 ),
                pDataWin->GetSizePixel() ) );
        }

        nFirstCol = nFirstCol + (sal_uInt16)nCols;
        aHScroll.SetThumbPos( nFirstCol - FrozenColCount() );
    }

    // adjust external headerbar, if necessary
    if ( getDataWindow()->pHeaderBar )
    {
        long nWidth = 0;
        for ( size_t nCol = 0;
              nCol < pCols->size() && nCol < nFirstCol;
              ++nCol )
        {
            // not the handle column
            if ( (*pCols)[ nCol ]->GetId() )
                nWidth += (*pCols)[ nCol ]->Width();
        }

        getDataWindow()->pHeaderBar->SetOffset( nWidth );
    }

    if( bInvalidateView )
    {
        Control::Invalidate( INVALIDATE_NOCHILDREN );
        pDataWin->Window::Invalidate( INVALIDATE_NOCHILDREN );
    }

    // implicitly show cursor after scrolling
    if ( nCols )
    {
        getDataWindow()->Update();
        Update();
    }
    bScrolling = false;
    EndScroll();

    return nCols;
}



long BrowseBox::ScrollRows( long nRows )
{

    // out of range?
    if ( getDataWindow()->bNoScrollBack && nRows < 0 )
        return 0;

    // compute new top row
    long nTmpMin = std::min( (long)(nTopRow + nRows), (long)(nRowCount - 1) );

    long nNewTopRow = std::max( (long)nTmpMin, (long)0 );

    if ( nNewTopRow == nTopRow )
        return 0;

    sal_uInt16 nVisibleRows =
        (sal_uInt16)(pDataWin->GetOutputSizePixel().Height() / GetDataRowHeight() + 1);

    VisibleRowsChanged(nNewTopRow, nVisibleRows);

    // compute new top row again (nTopRow might have changed!)
    nTmpMin = std::min( (long)(nTopRow + nRows), (long)(nRowCount - 1) );

    nNewTopRow = std::max( (long)nTmpMin, (long)0 );

    StartScroll();

    // scroll area on screen and/or repaint
    long nDeltaY = GetDataRowHeight() * ( nNewTopRow - nTopRow );
    long nOldTopRow = nTopRow;
    nTopRow = nNewTopRow;

    if ( GetUpdateMode() )
    {
        pVScroll->SetRange( Range( 0L, nRowCount ) );
        pVScroll->SetThumbPos( nTopRow );

        if( pDataWin->GetBackground().IsScrollable() &&
            std::abs( nDeltaY ) > 0 &&
            std::abs( nDeltaY ) < pDataWin->GetSizePixel().Height() )
        {
            pDataWin->Scroll( 0, (short)-nDeltaY, SCROLL_FLAGS );
        }
        else
            getDataWindow()->Invalidate();

        if ( nTopRow - nOldTopRow )
            getDataWindow()->Update();
    }

    EndScroll();

    return nTopRow - nOldTopRow;
}



void BrowseBox::RowModified( long nRow, sal_uInt16 nColId )
{

    if ( !GetUpdateMode() )
        return;

    Rectangle aRect;
    if ( nColId == BROWSER_INVALIDID )
        // invalidate the whole row
        aRect = Rectangle( Point( 0, (nRow-nTopRow) * GetDataRowHeight() ),
                    Size( pDataWin->GetSizePixel().Width(), GetDataRowHeight() ) );
    else
    {
        // invalidate the specific field
        aRect = GetFieldRectPixel( nRow, nColId, false );
    }
    getDataWindow()->Invalidate( aRect );
}



void BrowseBox::Clear()
{

    // adjust the total number of rows
    DoHideCursor( "Clear" );
    long nOldRowCount = nRowCount;
    nRowCount = 0;
    if(bMultiSelection)
    {
        assert(uRow.pSel);
        *uRow.pSel = MultiSelection();
    }
    else
        uRow.nSel = BROWSER_ENDOFSELECTION;
    nCurRow = BROWSER_ENDOFSELECTION;
    nTopRow = 0;
    nCurColId = 0;

    // nFirstCol may not be reset, else the scrolling code will become confused.
    // nFirstCol may only be changed when adding or deleting columns
    // nFirstCol = 0; -> wrong!
    aHScroll.SetThumbPos( 0 );
    pVScroll->SetThumbPos( 0 );

    Invalidate();
    UpdateScrollbars();
    SetNoSelection();
    DoShowCursor( "Clear" );
    CursorMoved();

    if ( isAccessibleAlive() )
    {
        // all rows should be removed, so we remove the row header bar and append it again
        // to avoid to notify every row remove
        if ( nOldRowCount != nRowCount )
        {
            commitBrowseBoxEvent(
                CHILD,
                Any(),
                makeAny( m_pImpl->getAccessibleHeaderBar( BBTYPE_ROWHEADERBAR ) )
            );

            // and now append it again
            commitBrowseBoxEvent(
                CHILD,
                makeAny( m_pImpl->getAccessibleHeaderBar( BBTYPE_ROWHEADERBAR ) ),
                Any()
            );

            // notify a table model change
            commitTableEvent(
                TABLE_MODEL_CHANGED,
                makeAny( AccessibleTableModelChange( DELETE,
                    0,
                    nOldRowCount,
                    0,
                    GetColumnCount())
                ),
                Any()
            );
        }
    }
}

void BrowseBox::RowInserted( long nRow, long nNumRows, bool bDoPaint, bool bKeepSelection )
{

    if (nRow < 0)
        nRow = 0;
    else if (nRow > nRowCount) // maximal = nRowCount
        nRow = nRowCount;

    if ( nNumRows <= 0 )
        return;

    // adjust total row count
    bool bLastRow = nRow >= nRowCount;
    nRowCount += nNumRows;

    DoHideCursor( "RowInserted" );

    // must we paint the new rows?
    long nOldCurRow = nCurRow;
    Size aSz = pDataWin->GetOutputSizePixel();
    if ( bDoPaint && nRow >= nTopRow &&
         nRow <= nTopRow + aSz.Height() / GetDataRowHeight() )
    {
        long nY = (nRow-nTopRow) * GetDataRowHeight();
        if ( !bLastRow )
        {
            // scroll down the rows behind the new row
            pDataWin->SetClipRegion();
            if( pDataWin->GetBackground().IsScrollable() )
            {
                pDataWin->Scroll( 0, GetDataRowHeight() * nNumRows,
                                Rectangle( Point( 0, nY ),
                                        Size( aSz.Width(), aSz.Height() - nY ) ),
                                SCROLL_FLAGS );
            }
            else
                pDataWin->Window::Invalidate( INVALIDATE_NOCHILDREN );
        }
        else
            // scroll would cause a repaint, so we must explicitly invalidate
            pDataWin->Invalidate( Rectangle( Point( 0, nY ),
                         Size( aSz.Width(), nNumRows * GetDataRowHeight() ) ) );
    }

    // correct top row if necessary
    if ( nRow < nTopRow )
        nTopRow += nNumRows;

    // adjust the selection
    if ( bMultiSelection )
        uRow.pSel->Insert( nRow, nNumRows );
    else if ( uRow.nSel != BROWSER_ENDOFSELECTION && nRow <= uRow.nSel )
        uRow.nSel += nNumRows;

    // adjust the cursor
    if ( nCurRow == BROWSER_ENDOFSELECTION )
        GoToRow( 0, false, bKeepSelection );
    else if ( nRow <= nCurRow )
        GoToRow( nCurRow += nNumRows, false, bKeepSelection );

    // adjust the vertical scrollbar
    if ( bDoPaint )
    {
        UpdateScrollbars();
        AutoSizeLastColumn();
    }

    DoShowCursor( "RowInserted" );
    // notify accessible that rows were inserted
    if ( isAccessibleAlive() )
    {
        commitTableEvent(
            TABLE_MODEL_CHANGED,
            makeAny( AccessibleTableModelChange(
                        INSERT,
                        nRow,
                        nRow + nNumRows,
                        0,
                        GetColumnCount()
                    )
            ),
            Any()
        );

        for (sal_Int32 i = nRow+1 ; i <= nRowCount ; ++i)
        {
            commitHeaderBarEvent(
                CHILD,
                makeAny( CreateAccessibleRowHeader( i ) ),
                Any(),
                false
            );
        }
    }

    if ( nCurRow != nOldCurRow )
        CursorMoved();

    DBG_ASSERT(nRowCount > 0,"BrowseBox: nRowCount <= 0");
    DBG_ASSERT(nCurRow >= 0,"BrowseBox: nCurRow < 0");
    DBG_ASSERT(nCurRow < nRowCount,"nCurRow >= nRowCount");
}



void BrowseBox::RowRemoved( long nRow, long nNumRows, bool bDoPaint )
{

    if ( nRow < 0 )
        nRow = 0;
    else if ( nRow >= nRowCount )
        nRow = nRowCount - 1;

    if ( nNumRows <= 0 )
        return;

    if ( nRowCount <= 0 )
        return;

    if ( bDoPaint )
    {
        // hide cursor and selection
        OSL_TRACE( "BrowseBox: %p->HideCursor", this );
        ToggleSelection();
        DoHideCursor( "RowRemoved" );
    }

    // adjust total row count
    nRowCount -= nNumRows;
    if (nRowCount < 0) nRowCount = 0;
    long nOldCurRow = nCurRow;

    // adjust the selection
    if ( bMultiSelection )
        // uRow.pSel->Remove( nRow, nNumRows );
        for ( long i = 0; i < nNumRows; i++ )
            uRow.pSel->Remove( nRow );
    else if ( nRow < uRow.nSel && uRow.nSel >= nNumRows )
        uRow.nSel -= nNumRows;
    else if ( nRow <= uRow.nSel )
        uRow.nSel = BROWSER_ENDOFSELECTION;

    // adjust the cursor
    if ( nRowCount == 0 )   // don't compare nRowCount with nNumRows as nNumRows already was subtracted from nRowCount
        nCurRow = BROWSER_ENDOFSELECTION;
    else if ( nRow < nCurRow )
    {
        nCurRow -= std::min( nCurRow - nRow, nNumRows );
        // with the above nCurRow points a) to the first row after the removed block or b) to the same line
        // as before, but moved up nNumRows
        // case a) needs an additional correction if the last n lines were deleted, as 'the first row after the
        // removed block' is an invalid position then
        // FS - 09/28/99 - 68429
        if (nCurRow == nRowCount)
            --nCurRow;
    }
    else if( nRow == nCurRow && nCurRow == nRowCount )
        nCurRow = nRowCount-1;

    // is the deleted row visible?
    Size aSz = pDataWin->GetOutputSizePixel();
    if ( nRow >= nTopRow &&
         nRow <= nTopRow + aSz.Height() / GetDataRowHeight() )
    {
        if ( bDoPaint )
        {
            // scroll up the rows behind the deleted row
            // if there are Rows behind
            if (nRow < nRowCount)
            {
                long nY = (nRow-nTopRow) * GetDataRowHeight();
                pDataWin->SetClipRegion();
                if( pDataWin->GetBackground().IsScrollable() )
                {
                    pDataWin->Scroll( 0, - (short) GetDataRowHeight() * nNumRows,
                        Rectangle( Point( 0, nY ), Size( aSz.Width(),
                            aSz.Height() - nY + nNumRows*GetDataRowHeight() ) ),
                            SCROLL_FLAGS );
                }
                else
                    pDataWin->Window::Invalidate( INVALIDATE_NOCHILDREN );
            }
            else
            {
                // Repaint the Rect of the deleted row
                Rectangle aRect(
                        Point( 0, (nRow-nTopRow)*GetDataRowHeight() ),
                        Size( pDataWin->GetSizePixel().Width(),
                              nNumRows * GetDataRowHeight() ) );
                pDataWin->Invalidate( aRect );
            }
        }
    }
    // is the deleted row above of the visible area?
    else if ( nRow < nTopRow )
        nTopRow = nTopRow >= nNumRows ? nTopRow-nNumRows : 0;

    if ( bDoPaint )
    {
        // reshow cursor and selection
        ToggleSelection();
        OSL_TRACE( "BrowseBox: %p->ShowCursor", this );
        DoShowCursor( "RowRemoved" );

        // adjust the vertical scrollbar
        UpdateScrollbars();
        AutoSizeLastColumn();
    }

    if ( isAccessibleAlive() )
    {
        if ( nRowCount == 0 )
        {
            // all columns should be removed, so we remove the column header bar and append it again
            // to avoid to notify every column remove
            commitBrowseBoxEvent(
                CHILD,
                Any(),
                makeAny( m_pImpl->getAccessibleHeaderBar( BBTYPE_ROWHEADERBAR ) )
            );

            // and now append it again
            commitBrowseBoxEvent(
                CHILD,
                makeAny(m_pImpl->getAccessibleHeaderBar(BBTYPE_ROWHEADERBAR)),
                Any()
            );
            commitBrowseBoxEvent(
                CHILD,
                Any(),
                makeAny( m_pImpl->getAccessibleTable() )
            );

            // and now append it again
            commitBrowseBoxEvent(
                CHILD,
                makeAny( m_pImpl->getAccessibleTable() ),
                Any()
            );
        }
        else
        {
            commitTableEvent(
                TABLE_MODEL_CHANGED,
                makeAny( AccessibleTableModelChange(
                            DELETE,
                            nRow,
                            nRow + nNumRows,
                            0,
                            GetColumnCount()
                            )
                ),
                Any()
            );

            for (sal_Int32 i = nRow+1 ; i <= (nRow+nNumRows) ; ++i)
            {
                commitHeaderBarEvent(
                    CHILD,
                    Any(),
                    makeAny( CreateAccessibleRowHeader( i ) ),
                    false
                );
            }
        }
    }

    if ( nOldCurRow != nCurRow )
        CursorMoved();

    DBG_ASSERT(nRowCount >= 0,"BrowseBox: nRowCount < 0");
    DBG_ASSERT(nCurRow >= 0 || nRowCount == 0,"BrowseBox: nCurRow < 0 && nRowCount != 0");
    DBG_ASSERT(nCurRow < nRowCount,"nCurRow >= nRowCount");
}



bool BrowseBox::GoToRow( long nRow)
{
    return GoToRow(nRow, false, false);
}



bool BrowseBox::GoToRow( long nRow, bool bRowColMove, bool bKeepSelection )
{

    long nOldCurRow = nCurRow;

    // nothing to do?
    if ( nRow == nCurRow && ( bMultiSelection || uRow.nSel == nRow ) )
        return true;

    // out of range?
    if ( nRow < 0 || nRow >= nRowCount )
        return false;

    // not allowed?
    if ( ( !bRowColMove && !IsCursorMoveAllowed( nRow, nCurColId ) ) )
        return false;

    if ( getDataWindow()->bNoScrollBack && nRow < nTopRow )
        nRow = nTopRow;

    // compute the last visible row
    Size aSz( pDataWin->GetSizePixel() );
    sal_uInt16 nVisibleRows = sal_uInt16( aSz.Height() / GetDataRowHeight() - 1 );
    long nLastRow = nTopRow + nVisibleRows;

    // suspend Updates
    getDataWindow()->EnterUpdateLock();

    // remove old highlight, if necessary
    if ( !bMultiSelection && !bKeepSelection )
        ToggleSelection();
    DoHideCursor( "GoToRow" );

    // must we scroll?
    bool bWasVisible = bSelectionIsVisible;
    if (! bMultiSelection)
    {
        if( !bKeepSelection )
            bSelectionIsVisible = false;
    }
    if ( nRow < nTopRow )
        ScrollRows( nRow - nTopRow );
    else if ( nRow > nLastRow )
        ScrollRows( nRow - nLastRow );
    bSelectionIsVisible = bWasVisible;

    // adjust cursor (selection) and thumb
    if ( GetUpdateMode() )
        pVScroll->SetThumbPos( nTopRow );

    // relative positioning (because nCurRow might have changed in the meantime)!
    if (nCurRow != BROWSER_ENDOFSELECTION )
        nCurRow = nCurRow + (nRow - nOldCurRow);

    // make sure that the current position is valid
    if (nCurRow == BROWSER_ENDOFSELECTION && nRowCount > 0)
        nCurRow = 0;
    else if ( nCurRow >= nRowCount )
        nCurRow = nRowCount - 1;
    aSelRange = Range( nCurRow, nCurRow );

    // display new highlight if necessary
    if ( !bMultiSelection && !bKeepSelection )
        uRow.nSel = nRow;

    // resume Updates
    getDataWindow()->LeaveUpdateLock();

    // Cursor+Highlight
    if ( !bMultiSelection && !bKeepSelection)
        ToggleSelection();
    DoShowCursor( "GoToRow" );
    if ( !bRowColMove  && nOldCurRow != nCurRow )
        CursorMoved();

    if ( !bMultiSelection && !bKeepSelection )
    {
        if ( !bSelecting )
            Select();
        else
            bSelect = true;
    }
    return true;
}



bool BrowseBox::GoToColumnId( sal_uInt16 nColId)
{
    return GoToColumnId(nColId, true, false);
}


bool BrowseBox::GoToColumnId( sal_uInt16 nColId, bool bMakeVisible, bool bRowColMove)
{
    if (!bColumnCursor)
        return false;

    // allowed?
    if (!bRowColMove && !IsCursorMoveAllowed( nCurRow, nColId ) )
        return false;

    if ( nColId != nCurColId || (bMakeVisible && !IsFieldVisible(nCurRow, nColId, true)))
    {
        sal_uInt16 nNewPos = GetColumnPos(nColId);
        BrowserColumn* pColumn = (nNewPos < pCols->size()) ? (*pCols)[ nNewPos ] : NULL;
        DBG_ASSERT( pColumn, "no column object - invalid id?" );
        if ( !pColumn )
            return false;

        DoHideCursor( "GoToColumnId" );
        nCurColId = nColId;

        bool bScrolled = false;

        sal_uInt16 nFirstPos = nFirstCol;
        sal_uInt16 nWidth = (sal_uInt16)pColumn->Width();
        sal_uInt16 nLastPos = GetColumnAtXPosPixel(
                            pDataWin->GetSizePixel().Width()-nWidth, false );
        sal_uInt16 nFrozen = FrozenColCount();
        if ( bMakeVisible && nLastPos &&
             nNewPos >= nFrozen && ( nNewPos < nFirstPos || nNewPos > nLastPos ) )
        {
            if ( nNewPos < nFirstPos )
                ScrollColumns( nNewPos-nFirstPos );
            else if ( nNewPos > nLastPos )
                ScrollColumns( nNewPos-nLastPos );
            bScrolled = true;
        }

        DoShowCursor( "GoToColumnId" );
        if (!bRowColMove)
        {
            //try to move to nCurRow, nColId
            CursorMoveAttempt aAttempt(nCurRow, nColId, bScrolled);
            //Detect if we are already in a call to BrowseBox::GoToColumnId
            //but the attempt is impossible and we are simply recursing
            //into BrowseBox::GoToColumnId with the same impossible to
            //fulfill conditions
            if (m_aGotoStack.empty() || aAttempt != m_aGotoStack.top())
            {
                m_aGotoStack.push(aAttempt);
                CursorMoved();
                m_aGotoStack.pop();
            }
        }
        return true;
    }
    return true;
}



bool BrowseBox::GoToRowColumnId( long nRow, sal_uInt16 nColId )
{

    // out of range?
    if ( nRow < 0 || nRow >= nRowCount )
        return false;

    if (!bColumnCursor)
        return false;

    // nothing to do ?
    if ( nRow == nCurRow && ( bMultiSelection || uRow.nSel == nRow ) &&
         nColId == nCurColId && IsFieldVisible(nCurRow, nColId, true))
        return true;

    // allowed?
    if (!IsCursorMoveAllowed(nRow, nColId))
        return false;

    DoHideCursor( "GoToRowColumnId" );
    bool bMoved = GoToRow(nRow, true) && GoToColumnId(nColId, true, true);
    DoShowCursor( "GoToRowColumnId" );

    if (bMoved)
        CursorMoved();

    return bMoved;
}



void BrowseBox::SetNoSelection()
{

    // is there no selection
    if ( ( !pColSel || !pColSel->GetSelectCount() ) &&
         ( ( !bMultiSelection && uRow.nSel == BROWSER_ENDOFSELECTION ) ||
           ( bMultiSelection && !uRow.pSel->GetSelectCount() ) ) )
        // nothing to do
        return;

    OSL_TRACE( "BrowseBox: %p->HideCursor", this );
    ToggleSelection();

    // unselect all
    if ( bMultiSelection )
        uRow.pSel->SelectAll(false);
    else
        uRow.nSel = BROWSER_ENDOFSELECTION;
    if ( pColSel )
        pColSel->SelectAll(false);
    if ( !bSelecting )
        Select();
    else
        bSelect = true;

    // restore screen
    OSL_TRACE( "BrowseBox: %p->ShowCursor", this );

    if ( isAccessibleAlive() )
    {
        commitTableEvent(
            SELECTION_CHANGED,
            Any(),
            Any()
        );
    }
}



void BrowseBox::SelectAll()
{

    if ( !bMultiSelection )
        return;

    OSL_TRACE( "BrowseBox: %p->HideCursor", this );
    ToggleSelection();

    // select all rows
    if ( pColSel )
        pColSel->SelectAll(false);
    uRow.pSel->SelectAll(true);

    // don't highlight handle column
    BrowserColumn *pFirstCol = (*pCols)[ 0 ];
    long nOfsX = pFirstCol->GetId() ? 0 : pFirstCol->Width();

    // highlight the row selection
    if ( !bHideSelect )
    {
        Rectangle aHighlightRect;
        sal_uInt16 nVisibleRows =
            (sal_uInt16)(pDataWin->GetOutputSizePixel().Height() / GetDataRowHeight() + 1);
        for ( long nRow = std::max( nTopRow, uRow.pSel->FirstSelected() );
              nRow != BROWSER_ENDOFSELECTION && nRow < nTopRow + nVisibleRows;
              nRow = uRow.pSel->NextSelected() )
            aHighlightRect.Union( Rectangle(
                Point( nOfsX, (nRow-nTopRow)*GetDataRowHeight() ),
                Size( pDataWin->GetSizePixel().Width(), GetDataRowHeight() ) ) );
        pDataWin->Invalidate( aHighlightRect );
    }

    if ( !bSelecting )
        Select();
    else
        bSelect = true;

    // restore screen
    OSL_TRACE( "BrowseBox: %p->ShowCursor", this );

    if ( isAccessibleAlive() )
    {
        commitTableEvent(
            SELECTION_CHANGED,
            Any(),
            Any()
        );
        commitHeaderBarEvent(
            SELECTION_CHANGED,
            Any(),
            Any(),
            true
        ); // column header event

        commitHeaderBarEvent(
            SELECTION_CHANGED,
            Any(),
            Any(),
            false
        ); // row header event
    }
}



void BrowseBox::SelectRow( long nRow, bool _bSelect, bool bExpand )
{

    if ( !bMultiSelection )
    {
        // deselecting is impossible, selecting via cursor
        if ( _bSelect )
            GoToRow(nRow, false);
        return;
    }

    OSL_TRACE( "BrowseBox: %p->HideCursor", this );

    // remove old selection?
    if ( !bExpand || !bMultiSelection )
    {
        ToggleSelection();
        if ( bMultiSelection )
            uRow.pSel->SelectAll(false);
        else
            uRow.nSel = BROWSER_ENDOFSELECTION;
        if ( pColSel )
            pColSel->SelectAll(false);
    }

    // set new selection
    if  (   !bHideSelect
        &&  (   (   bMultiSelection
                &&  uRow.pSel->GetTotalRange().Max() >= nRow
                &&  uRow.pSel->Select( nRow, _bSelect )
                )
            ||  (   !bMultiSelection
                &&  ( uRow.nSel = nRow ) != BROWSER_ENDOFSELECTION )
                )
            )
    {
        // don't highlight handle column
        BrowserColumn *pFirstCol = (*pCols)[ 0 ];
        long nOfsX = pFirstCol->GetId() ? 0 : pFirstCol->Width();

        // highlight only newly selected part
        Rectangle aRect(
            Point( nOfsX, (nRow-nTopRow)*GetDataRowHeight() ),
            Size( pDataWin->GetSizePixel().Width(), GetDataRowHeight() ) );
        pDataWin->Invalidate( aRect );
    }

    if ( !bSelecting )
        Select();
    else
        bSelect = true;

    // restore screen
    OSL_TRACE( "BrowseBox: %p->ShowCursor", this );

    if ( isAccessibleAlive() )
    {
        commitTableEvent(
            SELECTION_CHANGED,
            Any(),
            Any()
        );
        commitHeaderBarEvent(
            SELECTION_CHANGED,
            Any(),
            Any(),
            false
        ); // row header event
    }
}



long BrowseBox::GetSelectRowCount() const
{

    return bMultiSelection ? uRow.pSel->GetSelectCount() :
           uRow.nSel == BROWSER_ENDOFSELECTION ? 0 : 1;
}



void BrowseBox::SelectColumnPos( sal_uInt16 nNewColPos, bool _bSelect, bool bMakeVisible )
{

    if ( !bColumnCursor || nNewColPos == BROWSER_INVALIDID )
        return;

    if ( !bMultiSelection )
    {
        if ( _bSelect )
            GoToColumnId( (*pCols)[ nNewColPos ]->GetId(), bMakeVisible );
        return;
    }
    else
    {
        if ( !GoToColumnId( (*pCols)[ nNewColPos ]->GetId(), bMakeVisible ) )
            return;
    }

    OSL_TRACE( "BrowseBox: %p->HideCursor", this );
    ToggleSelection();
    if ( bMultiSelection )
        uRow.pSel->SelectAll(false);
    else
        uRow.nSel = BROWSER_ENDOFSELECTION;
    pColSel->SelectAll(false);

    if ( pColSel->Select( nNewColPos, _bSelect ) )
    {
        // GoToColumnId( pCols->GetObject(nNewColPos)->GetId(), bMakeVisible );

        // only highlight painted areas
        pDataWin->Update();
        Rectangle aFieldRectPix( GetFieldRectPixel( nCurRow, nCurColId, false ) );
        Rectangle aRect(
            Point( aFieldRectPix.Left() - MIN_COLUMNWIDTH, 0 ),
            Size( (*pCols)[ nNewColPos ]->Width(),
                  pDataWin->GetOutputSizePixel().Height() ) );
        pDataWin->Invalidate( aRect );
        if ( !bSelecting )
            Select();
        else
            bSelect = true;

        if ( isAccessibleAlive() )
        {
            commitTableEvent(
                SELECTION_CHANGED,
                Any(),
                Any()
            );
            commitHeaderBarEvent(
                SELECTION_CHANGED,
                Any(),
                Any(),
                true
            ); // column header event
        }
    }

    // restore screen
    OSL_TRACE( "BrowseBox: %p->ShowCursor", this );
}



sal_uInt16 BrowseBox::GetSelectColumnCount() const
{

    // while bAutoSelect (==!pColSel), 1 if any rows (yes rows!) else none
    return pColSel ? (sal_uInt16) pColSel->GetSelectCount() :
           nCurRow >= 0 ? 1 : 0;
}


long BrowseBox::FirstSelectedColumn( ) const
{
    return pColSel ? pColSel->FirstSelected() : BROWSER_ENDOFSELECTION;
}



long BrowseBox::FirstSelectedRow( bool bInverse )
{

    return bMultiSelection ? uRow.pSel->FirstSelected(bInverse) : uRow.nSel;
}



long BrowseBox::NextSelectedRow()
{

    return bMultiSelection ? uRow.pSel->NextSelected() : BROWSER_ENDOFSELECTION;
}



long BrowseBox::LastSelectedRow()
{

    return bMultiSelection ? uRow.pSel->LastSelected() : uRow.nSel;
}



bool BrowseBox::IsRowSelected( long nRow ) const
{

    return bMultiSelection ? uRow.pSel->IsSelected(nRow) : nRow == uRow.nSel;
}



bool BrowseBox::IsColumnSelected( sal_uInt16 nColumnId ) const
{

    return pColSel ? pColSel->IsSelected( GetColumnPos(nColumnId) ) :
                     nCurColId == nColumnId;
}



bool BrowseBox::MakeFieldVisible
(
    long    nRow,       // line number of the field (starting with 0)
    sal_uInt16  nColId,     // column ID of the field
    bool    bComplete   // (== false), true => make visible in its entirety
)

/*  [Description]

    Makes visible the field described in 'nRow' and 'nColId' by scrolling
    accordingly. If 'bComplete' is set, the field should become visible in its
    entirety.

    [Returned Value]

    bool            true
                    The given field is already visible or was already visible.

                    false
                    The given field could not be made visible or in the case of
                    'bComplete' could not be made visible in its entirety.
*/

{
    Size aTestSize = pDataWin->GetSizePixel();

    if ( !bBootstrapped ||
         ( aTestSize.Width() == 0 && aTestSize.Height() == 0 ) )
        return false;

    // is it visible already?
    bool bVisible = IsFieldVisible( nRow, nColId, bComplete );
    if ( bVisible )
        return true;

    // calculate column position, field rectangle and painting area
    sal_uInt16 nColPos = GetColumnPos( nColId );
    Rectangle aFieldRect = GetFieldRectPixel( nRow, nColId, false );
    Rectangle aDataRect = Rectangle( Point(0, 0), pDataWin->GetSizePixel() );

    // positioned outside on the left?
    if ( nColPos >= FrozenColCount() && nColPos < nFirstCol )
        // => scroll to the right
        ScrollColumns( nColPos - nFirstCol );

    // while outside on the right
    while ( aDataRect.Right() < ( bComplete
                ? aFieldRect.Right()
                : aFieldRect.Left()+aFieldRect.GetWidth()/2 ) )
    {
        // => scroll to the left
        if ( ScrollColumns( 1 ) != 1 )
            // no more need to scroll
            break;
        aFieldRect = GetFieldRectPixel( nRow, nColId, false );
    }

    // positioned outside above?
    if ( nRow < nTopRow )
        // scroll further to the bottom
        ScrollRows( nRow - nTopRow );

    // positioned outside below?
    long nBottomRow = nTopRow + GetVisibleRows();
    // decrement nBottomRow to make it the number of the last visible line
    // (count starts with 0!).
    // Example: BrowseBox contains exactly one entry. nBottomRow := 0 + 1 - 1
    if( nBottomRow )
        nBottomRow--;

    if ( nRow > nBottomRow )
        // scroll further to the top
        ScrollRows( nRow - nBottomRow );

    // it might still not actually fit, e.g. if the window is too small
    return IsFieldVisible( nRow, nColId, bComplete );
}



bool BrowseBox::IsFieldVisible( long nRow, sal_uInt16 nColumnId,
                                bool bCompletely ) const
{

    // hidden by frozen column?
    sal_uInt16 nColPos = GetColumnPos( nColumnId );
    if ( nColPos >= FrozenColCount() && nColPos < nFirstCol )
        return false;

    Rectangle aRect( ImplFieldRectPixel( nRow, nColumnId ) );
    if ( aRect.IsEmpty() )
        return false;

    // get the visible area
    Rectangle aOutRect( Point(0, 0), pDataWin->GetOutputSizePixel() );

    if ( bCompletely )
        // test if the field is completely visible
        return aOutRect.IsInside( aRect );
    else
        // test if the field is partly of completely visible
        return !aOutRect.Intersection( aRect ).IsEmpty();
}



Rectangle BrowseBox::GetFieldRectPixel( long nRow, sal_uInt16 nColumnId,
                                        bool bRelToBrowser) const
{

    // get the rectangle relative to DataWin
    Rectangle aRect( ImplFieldRectPixel( nRow, nColumnId ) );
    if ( aRect.IsEmpty() )
        return aRect;

    // adjust relative to BrowseBox's output area
    Point aTopLeft( aRect.TopLeft() );
    if ( bRelToBrowser )
    {
        aTopLeft = pDataWin->OutputToScreenPixel( aTopLeft );
        aTopLeft = ScreenToOutputPixel( aTopLeft );
    }

    return Rectangle( aTopLeft, aRect.GetSize() );
}



Rectangle BrowseBox::GetRowRectPixel( long nRow, bool bRelToBrowser  ) const
{

    // get the rectangle relative to DataWin
    Rectangle aRect;
    if ( nTopRow > nRow )
        // row is above visible area
        return aRect;
    aRect = Rectangle(
        Point( 0, GetDataRowHeight() * (nRow-nTopRow) ),
        Size( pDataWin->GetOutputSizePixel().Width(), GetDataRowHeight() ) );
    if ( aRect.TopLeft().Y() > pDataWin->GetOutputSizePixel().Height() )
        // row is below visible area
        return aRect;

    // adjust relative to BrowseBox's output area
    Point aTopLeft( aRect.TopLeft() );
    if ( bRelToBrowser )
    {
        aTopLeft = pDataWin->OutputToScreenPixel( aTopLeft );
        aTopLeft = ScreenToOutputPixel( aTopLeft );
    }

    return Rectangle( aTopLeft, aRect.GetSize() );
}



Rectangle BrowseBox::ImplFieldRectPixel( long nRow, sal_uInt16 nColumnId ) const
{

    // compute the X-coordinate relative to DataWin by accumulation
    long nColX = 0;
    sal_uInt16 nFrozenCols = FrozenColCount();
    size_t nCol;
    for ( nCol = 0;
          nCol < pCols->size() && (*pCols)[ nCol ]->GetId() != nColumnId;
          ++nCol )
        if ( (*pCols)[ nCol ]->IsFrozen() || nCol >= nFirstCol )
            nColX += (*pCols)[ nCol ]->Width();

    if ( nCol >= pCols->size() || ( nCol >= nFrozenCols && nCol < nFirstCol ) )
        return Rectangle();

    // compute the Y-coordinate relative to DataWin
    long nRowY = GetDataRowHeight();
    if ( nRow != BROWSER_ENDOFSELECTION ) // #105497# OJ
        nRowY = ( nRow - nTopRow ) * GetDataRowHeight();

    // assemble the Rectangle relative to DataWin
    return Rectangle(
        Point( nColX + MIN_COLUMNWIDTH, nRowY ),
        Size( (*pCols)[ nCol ]->Width() - 2*MIN_COLUMNWIDTH,
              GetDataRowHeight() - 1 ) );
}



long BrowseBox::GetRowAtYPosPixel( long nY, bool bRelToBrowser ) const
{

    // compute the Y-coordinate
    if ( bRelToBrowser )
    {
        Point aDataTopLeft = pDataWin->OutputToScreenPixel( Point(0, 0) );
        Point aTopLeft = OutputToScreenPixel( Point(0, 0) );
        nY -= aDataTopLeft.Y() - aTopLeft.Y();
    }

    // no row there (e.g. in the header)
    if ( nY < 0 || nY >= pDataWin->GetOutputSizePixel().Height() )
        return -1;

    return nY / GetDataRowHeight() + nTopRow;
}



Rectangle BrowseBox::GetFieldRect( sal_uInt16 nColumnId ) const
{

    return GetFieldRectPixel( nCurRow, nColumnId );
}



sal_uInt16 BrowseBox::GetColumnAtXPosPixel( long nX, bool ) const
{

    // accumulate the widths of the visible columns
    long nColX = 0;
    for ( size_t nCol = 0; nCol < pCols->size(); ++nCol )
    {
        BrowserColumn *pCol = (*pCols)[ nCol ];
        if ( pCol->IsFrozen() || nCol >= nFirstCol )
            nColX += pCol->Width();

        if ( nColX > nX )
            return nCol;
    }

    return BROWSER_INVALIDID;
}



void BrowseBox::ReserveControlArea( sal_uInt16 nWidth )
{

    if ( nWidth != nControlAreaWidth )
    {
        OSL_ENSURE(nWidth,"Control area of 0 is not allowed, Use USHRT_MAX instead!");
        nControlAreaWidth = nWidth;
        UpdateScrollbars();
    }
}



Rectangle BrowseBox::GetControlArea() const
{

    return Rectangle(
        Point( 0, GetOutputSizePixel().Height() - aHScroll.GetSizePixel().Height() ),
        Size( GetOutputSizePixel().Width() - aHScroll.GetSizePixel().Width(),
             aHScroll.GetSizePixel().Height() ) );
}



void BrowseBox::SetMode( BrowserMode nMode )
{

    getDataWindow()->bAutoHScroll = BROWSER_AUTO_HSCROLL == ( nMode & BROWSER_AUTO_HSCROLL );
    getDataWindow()->bAutoVScroll = BROWSER_AUTO_VSCROLL == ( nMode & BROWSER_AUTO_VSCROLL );
    getDataWindow()->bNoHScroll   = BROWSER_NO_HSCROLL   == ( nMode & BROWSER_NO_HSCROLL );
    getDataWindow()->bNoVScroll   = BROWSER_NO_VSCROLL   == ( nMode & BROWSER_NO_VSCROLL );

    DBG_ASSERT( !( getDataWindow()->bAutoHScroll && getDataWindow()->bNoHScroll ),
        "BrowseBox::SetMode: AutoHScroll *and* NoHScroll?" );
    DBG_ASSERT( !( getDataWindow()->bAutoVScroll && getDataWindow()->bNoVScroll ),
        "BrowseBox::SetMode: AutoVScroll *and* NoVScroll?" );
    if ( getDataWindow()->bAutoHScroll )
        getDataWindow()->bNoHScroll = false;
    if ( getDataWindow()->bAutoVScroll )
        getDataWindow()->bNoVScroll = false;

    if ( getDataWindow()->bNoHScroll )
        aHScroll.Hide();

    nControlAreaWidth = USHRT_MAX;

    getDataWindow()->bNoScrollBack =
            BROWSER_NO_SCROLLBACK == ( nMode & BROWSER_NO_SCROLLBACK);

    long nOldRowSel = bMultiSelection ? uRow.pSel->FirstSelected() : uRow.nSel;
    MultiSelection *pOldRowSel = bMultiSelection ? uRow.pSel : 0;
    MultiSelection *pOldColSel = pColSel;

    delete pVScroll;

    bThumbDragging = ( nMode & BROWSER_THUMBDRAGGING ) == BROWSER_THUMBDRAGGING;
    bMultiSelection = ( nMode & BROWSER_MULTISELECTION ) == BROWSER_MULTISELECTION;
    bColumnCursor = ( nMode & BROWSER_COLUMNSELECTION ) == BROWSER_COLUMNSELECTION;
    bKeepHighlight = ( nMode & BROWSER_KEEPSELECTION ) == BROWSER_KEEPSELECTION;

    bHideSelect = ((nMode & BROWSER_HIDESELECT) == BROWSER_HIDESELECT);
    // default: do not hide the cursor at all (untaken scrolling and such)
    bHideCursor = TRISTATE_FALSE;

    if ( BROWSER_SMART_HIDECURSOR == ( nMode & BROWSER_SMART_HIDECURSOR ) )
    {   // smart cursor hide overrules hard cursor hide
        bHideCursor = TRISTATE_INDET;
    }
    else if ( BROWSER_HIDECURSOR == ( nMode & BROWSER_HIDECURSOR ) )
    {
        bHideCursor = TRISTATE_TRUE;
    }

    m_bFocusOnlyCursor = ((nMode & BROWSER_CURSOR_WO_FOCUS) == 0);

    bHLines = ( nMode & BROWSER_HLINESFULL ) == BROWSER_HLINESFULL;
    bVLines = ( nMode & BROWSER_VLINESFULL ) == BROWSER_VLINESFULL;
    bHDots  = ( nMode & BROWSER_HLINESDOTS ) == BROWSER_HLINESDOTS;
    bVDots  = ( nMode & BROWSER_VLINESDOTS ) == BROWSER_VLINESDOTS;

    WinBits nVScrollWinBits =
        WB_VSCROLL | ( ( nMode & BROWSER_THUMBDRAGGING ) ? WB_DRAG : 0 );
    pVScroll = ( nMode & BROWSER_TRACKING_TIPS ) == BROWSER_TRACKING_TIPS
                ? new BrowserScrollBar( this, nVScrollWinBits,
                                        static_cast<BrowserDataWin*>( pDataWin ) )
                : new ScrollBar( this, nVScrollWinBits );
    pVScroll->SetLineSize( 1 );
    pVScroll->SetPageSize(1);
    pVScroll->SetScrollHdl( LINK( this, BrowseBox, ScrollHdl ) );
    pVScroll->SetEndScrollHdl( LINK( this, BrowseBox, EndScrollHdl ) );

    getDataWindow()->bAutoSizeLastCol =
            BROWSER_AUTOSIZE_LASTCOL == ( nMode & BROWSER_AUTOSIZE_LASTCOL );
    getDataWindow()->bOwnDataChangedHdl =
            BROWSER_OWN_DATACHANGED == ( nMode & BROWSER_OWN_DATACHANGED );

    // create a headerbar. what happens, if a headerbar has to be created and
    // there already are columns?
    if ( BROWSER_HEADERBAR_NEW == ( nMode & BROWSER_HEADERBAR_NEW ) )
    {
        if (!getDataWindow()->pHeaderBar)
            getDataWindow()->pHeaderBar = CreateHeaderBar( this );
    }
    else
    {
        DELETEZ(getDataWindow()->pHeaderBar);
    }



    if ( bColumnCursor )
    {
        pColSel = pOldColSel ? pOldColSel : new MultiSelection;
        pColSel->SetTotalRange( Range( 0, pCols->size()-1 ) );
    }
    else
    {
        delete pColSel;
        pColSel = 0;
    }

    if ( bMultiSelection )
    {
        if ( pOldRowSel )
            uRow.pSel = pOldRowSel;
        else
            uRow.pSel = new MultiSelection;
    }
    else
    {
        uRow.nSel = nOldRowSel;
        delete pOldRowSel;
    }

    if ( bBootstrapped )
    {
        StateChanged( StateChangedType::INITSHOW );
        if ( bMultiSelection && !pOldRowSel &&
             nOldRowSel != BROWSER_ENDOFSELECTION )
            uRow.pSel->Select( nOldRowSel );
    }

    if ( pDataWin )
        pDataWin->Invalidate();

    // no cursor on handle column
    if ( nCurColId == HandleColumnId )
        nCurColId = GetColumnId( 1 );

    m_nCurrentMode = nMode;
}



void BrowseBox::VisibleRowsChanged( long, sal_uInt16 )
{

    // old behavior: automatically correct NumRows:
    if ( nRowCount < GetRowCount() )
    {
        RowInserted(nRowCount,GetRowCount() - nRowCount, false);
    }
    else if ( nRowCount > GetRowCount() )
    {
        RowRemoved(nRowCount-(nRowCount - GetRowCount()),nRowCount - GetRowCount(), false);
    }
}



bool BrowseBox::IsCursorMoveAllowed( long, sal_uInt16 ) const

/*  [Description]

    This virtual method is always called before the cursor is moved directly.
    By means of 'return sal_False', we avoid doing this if e.g. a record
    contradicts any rules.

    This method is not called, if the cursor movement results from removing or
    deleting a row/column (thus, in cases where only a "cursor correction" happens).

    The base implementation currently always returns true.
*/

{
    return true;
}



long BrowseBox::GetDataRowHeight() const
{
    return CalcZoom(nDataRowHeight ? nDataRowHeight : ImpGetDataRowHeight());
}



BrowserHeader* BrowseBox::CreateHeaderBar( BrowseBox* pParent )
{
    BrowserHeader* pNewBar = new BrowserHeader( pParent );
    pNewBar->SetStartDragHdl( LINK( this, BrowseBox, StartDragHdl ) );
    return pNewBar;
}

void BrowseBox::SetHeaderBar( BrowserHeader* pHeaderBar )
{
    delete static_cast<BrowserDataWin*>(pDataWin)->pHeaderBar;
    static_cast<BrowserDataWin*>( pDataWin )->pHeaderBar = pHeaderBar;
    static_cast<BrowserDataWin*>( pDataWin )->pHeaderBar->SetStartDragHdl( LINK( this, BrowseBox, StartDragHdl ) );
}

long BrowseBox::GetTitleHeight() const
{
    long nHeight;
    // ask the header bar for the text height (if possible), as the header bar's font is adjusted with
    // our (and the header's) zoom factor
    HeaderBar* pHeaderBar = static_cast<BrowserDataWin*>( pDataWin )->pHeaderBar;
    if ( pHeaderBar )
        nHeight = pHeaderBar->GetTextHeight();
    else
        nHeight = GetTextHeight();

    return nTitleLines ? nTitleLines * nHeight + 4 : 0;
}

long BrowseBox::CalcReverseZoom(long nVal)
{
    if (IsZoom())
    {
        const Fraction& rZoom = GetZoom();
        double n = (double)nVal;
        n *= (double)rZoom.GetDenominator();
        if (!rZoom.GetNumerator())
            throw o3tl::divide_by_zero();
        n /= (double)rZoom.GetNumerator();
        nVal = n>0 ? (long)(n + 0.5) : -(long)(-n + 0.5);
    }

    return nVal;
}

void BrowseBox::CursorMoved()
{
    // before implementing more here, please adjust the EditBrowseBox

    if ( isAccessibleAlive() && HasFocus() )
        commitTableEvent(
            ACTIVE_DESCENDANT_CHANGED,
            makeAny( CreateAccessibleCell( GetCurRow(),GetColumnPos( GetCurColumnId() ) ) ),
            Any()
        );
}

void BrowseBox::LoseFocus()
{
    OSL_TRACE( "BrowseBox: %p->LoseFocus", this );

    if ( bHasFocus )
    {
        OSL_TRACE( "BrowseBox: %p->HideCursor", this );
        DoHideCursor( "LoseFocus" );

        if ( !bKeepHighlight )
        {
            ToggleSelection();
            bSelectionIsVisible = false;
        }

        bHasFocus = false;
    }
    Control::LoseFocus();
}



void BrowseBox::GetFocus()
{
    OSL_TRACE( "BrowseBox: %p->GetFocus", this );

    if ( !bHasFocus )
    {
        if ( !bSelectionIsVisible )
        {
            bSelectionIsVisible = true;
            if ( bBootstrapped )
                ToggleSelection();
        }

        bHasFocus = true;
        DoShowCursor( "GetFocus" );
    }
    Control::GetFocus();
}


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */