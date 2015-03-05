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

#ifndef INCLUDED_SW_SOURCE_CORE_INC_UNDOATTRIBUTE_HXX
#define INCLUDED_SW_SOURCE_CORE_INC_UNDOATTRIBUTE_HXX

#include <undobj.hxx>
#include <memory>
#include <rtl/ustring.hxx>
#include <svl/itemset.hxx>
#include <swtypes.hxx>
#include <calbck.hxx>
#include <set>

class SvxTabStopItem;
class SwFmt;
class SwFtnInfo;
class SwEndNoteInfo;

class SwUndoAttr : public SwUndo, private SwUndRng
{
    SfxItemSet m_AttrSet;                           // attributes for Redo
    const ::std::unique_ptr<SwHistory> m_pHistory;    // History for Undo
    ::std::unique_ptr<SwRedlineData> m_pRedlineData;  // Redlining
    ::std::unique_ptr<SwRedlineSaveDatas> m_pRedlineSaveData;
    sal_uLong m_nNodeIndex;                         // Offset: for Redlining
    const SetAttrMode m_nInsertFlags;               // insert flags

    void RemoveIdx( SwDoc& rDoc );

public:
    SwUndoAttr( const SwPaM&, const SfxItemSet &, const SetAttrMode nFlags );
    SwUndoAttr( const SwPaM&, const SfxPoolItem&, const SetAttrMode nFlags );

    virtual ~SwUndoAttr();

    virtual void UndoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RedoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RepeatImpl( ::sw::RepeatContext & ) SAL_OVERRIDE;

    void SaveRedlineData( const SwPaM& rPam, bool bInsCntnt );

    SwHistory& GetHistory() { return *m_pHistory; }
};

class SwUndoResetAttr : public SwUndo, private SwUndRng
{
    const ::std::unique_ptr<SwHistory> m_pHistory;
    std::set<sal_uInt16> m_Ids;
    const sal_uInt16 m_nFormatId;             // Format-Id for Redo

public:
    SwUndoResetAttr( const SwPaM&, sal_uInt16 nFmtId );
    SwUndoResetAttr( const SwPosition&, sal_uInt16 nFmtId );

    virtual ~SwUndoResetAttr();

    virtual void UndoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RedoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RepeatImpl( ::sw::RepeatContext & ) SAL_OVERRIDE;

    void SetAttrs( const std::set<sal_uInt16> &rAttrs );

    SwHistory& GetHistory() { return *m_pHistory; }
};

class SwUndoFmtAttr : public SwUndo
{
    friend class SwUndoDefaultAttr;
    SwFmt * m_pFmt;
    ::std::unique_ptr<SfxItemSet> m_pOldSet;    // old attributes
    sal_uLong m_nNodeIndex;
    const sal_uInt16 m_nFmtWhich;
    const bool m_bSaveDrawPt;

    bool IsFmtInDoc( SwDoc* );   //is the attribute format still in the Doc?
    void SaveFlyAnchor( bool bSaveDrawPt = false );
    // #i35443# - Add return value, type <bool>.
    // Return value indicates, if anchor attribute is restored.
    // Notes: - If anchor attribute is restored, all other existing attributes
    //          are also restored.
    //        - Anchor attribute isn't restored successfully, if it contains
    //          an invalid anchor position and all other existing attributes
    //          aren't restored.
    //          This situation occurs for undo of styles.
    bool RestoreFlyAnchor(::sw::UndoRedoContext & rContext);
    // --> OD 2008-02-27 #refactorlists# - removed <rAffectedItemSet>
    void Init();

public:
    // register at the Format and save old attributes
    // --> OD 2008-02-27 #refactorlists# - removed <rNewSet>
    SwUndoFmtAttr( const SfxItemSet& rOldSet,
                   SwFmt& rFmt,
                   bool bSaveDrawPt = true );
    SwUndoFmtAttr( const SfxPoolItem& rItem,
                   SwFmt& rFmt,
                   bool bSaveDrawPt = true );

    virtual ~SwUndoFmtAttr();

    virtual void UndoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RedoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RepeatImpl( ::sw::RepeatContext & ) SAL_OVERRIDE;

    virtual SwRewriter GetRewriter() const SAL_OVERRIDE;

    void PutAttr( const SfxPoolItem& rItem );
    SwFmt* GetFmt( SwDoc& rDoc );   // checks if it is still in the Doc!
};

// --> OD 2008-02-12 #newlistlevelattrs#
class SwUndoFmtResetAttr : public SwUndo
{
    public:
        SwUndoFmtResetAttr( SwFmt& rChangedFormat,
                            const sal_uInt16 nWhichId );
        virtual ~SwUndoFmtResetAttr();

        virtual void UndoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
        virtual void RedoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;

    private:
        // format at which a certain attribute is reset.
        SwFmt * const m_pChangedFormat;
        // which ID of the reset attribute
        const sal_uInt16 m_nWhichId;
        // old attribute which has been reset - needed for undo.
        ::std::unique_ptr<SfxPoolItem> m_pOldItem;
};

class SwUndoDontExpandFmt : public SwUndo
{
    const sal_uLong m_nNodeIndex;
    const sal_Int32 m_nContentIndex;

public:
    SwUndoDontExpandFmt( const SwPosition& rPos );

    virtual void UndoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RedoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RepeatImpl( ::sw::RepeatContext & ) SAL_OVERRIDE;
};

// helper class to receive changed attribute sets
class SwUndoFmtAttrHelper : public SwClient
{
    ::std::unique_ptr<SwUndoFmtAttr> m_pUndo;
    const bool m_bSaveDrawPt;

public:
    SwUndoFmtAttrHelper( SwFmt& rFmt, bool bSaveDrawPt = true );

    virtual void Modify( const SfxPoolItem*, const SfxPoolItem* ) SAL_OVERRIDE;

    SwUndoFmtAttr* GetUndo() const  { return m_pUndo.get(); }
    // release the undo object (so it is not deleted here), and return it
    SwUndoFmtAttr* ReleaseUndo()    { return m_pUndo.release(); }
};

class SwUndoMoveLeftMargin : public SwUndo, private SwUndRng
{
    const ::std::unique_ptr<SwHistory> m_pHistory;
    const bool m_bModulus;

public:
    SwUndoMoveLeftMargin( const SwPaM&, bool bRight, bool bModulus );

    virtual ~SwUndoMoveLeftMargin();

    virtual void UndoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RedoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RepeatImpl( ::sw::RepeatContext & ) SAL_OVERRIDE;

    SwHistory& GetHistory() { return *m_pHistory; }

};

class SwUndoDefaultAttr : public SwUndo
{
    ::std::unique_ptr<SfxItemSet> m_pOldSet;        // the old attributes
    ::std::unique_ptr<SvxTabStopItem> m_pTabStop;

public:
    // registers at the format and saves old attributes
    SwUndoDefaultAttr( const SfxItemSet& rOldSet );

    virtual ~SwUndoDefaultAttr();

    virtual void UndoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RedoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
};

class SwUndoChangeFootNote : public SwUndo, private SwUndRng
{
    const ::std::unique_ptr<SwHistory> m_pHistory;
    const OUString m_Text;
    const sal_uInt16 m_nNumber;
    const bool m_bEndNote;

public:
    SwUndoChangeFootNote( const SwPaM& rRange, const OUString& rTxt,
                          sal_uInt16 nNum, bool bIsEndNote );
    virtual ~SwUndoChangeFootNote();

    virtual void UndoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RedoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RepeatImpl( ::sw::RepeatContext & ) SAL_OVERRIDE;

    SwHistory& GetHistory() { return *m_pHistory; }
};

class SwUndoFootNoteInfo : public SwUndo
{
    ::std::unique_ptr<SwFtnInfo> m_pFootNoteInfo;

public:
    SwUndoFootNoteInfo( const SwFtnInfo &rInfo );

    virtual ~SwUndoFootNoteInfo();

    virtual void UndoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RedoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
};

class SwUndoEndNoteInfo : public SwUndo
{
    ::std::unique_ptr<SwEndNoteInfo> m_pEndNoteInfo;

public:
    SwUndoEndNoteInfo( const SwEndNoteInfo &rInfo );

    virtual ~SwUndoEndNoteInfo();

    virtual void UndoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
    virtual void RedoImpl( ::sw::UndoRedoContext & ) SAL_OVERRIDE;
};

#endif // INCLUDED_SW_SOURCE_CORE_INC_UNDOATTRIBUTE_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */