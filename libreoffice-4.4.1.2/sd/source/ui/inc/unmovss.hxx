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

#ifndef INCLUDED_SD_SOURCE_UI_INC_UNMOVSS_HXX
#define INCLUDED_SD_SOURCE_UI_INC_UNMOVSS_HXX

#include "sdundo.hxx"
#include <stlsheet.hxx>
#include <vector>

class SdDrawDocument;

class SdMoveStyleSheetsUndoAction : public SdUndoAction
{
    SdStyleSheetVector                  maStyles;
    std::vector< SdStyleSheetVector >   maListOfChildLists;
    bool                                mbMySheets;

public:
    SdMoveStyleSheetsUndoAction(SdDrawDocument* pTheDoc, SdStyleSheetVector& rTheStyles, bool bInserted);

    virtual ~SdMoveStyleSheetsUndoAction();
    virtual void Undo() SAL_OVERRIDE;
    virtual void Redo() SAL_OVERRIDE;

    virtual OUString GetComment() const SAL_OVERRIDE;
};

#endif // INCLUDED_SD_SOURCE_UI_INC_UNMOVSS_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */