/*=========================================================================

   Program: ParaView
   Module:    pqCheckableHeaderView.h

   Copyright (c) 2005-2008 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2.

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/

/// \file pqCheckableHeaderView.h
/// \date 03/04/2014

#ifndef _pqCheckableHeaderView_h
#define _pqCheckableHeaderView_h


#include "pqWidgetsModule.h"
#include <QHeaderView>

/// Pimpl class that handles drawing of all the section header checkboxes and
/// stores states of the checkboxes
class pqCheckableHeaderViewInternal;

/**
 * A convenience QHeaderView painted with a QCheckBox
 * This allows for providing a global checkbox when the model items are user
 * checkable.
 * The checkbox is painted per section, one of the three states (checked,
 * partially checked, unchecked) depending on the check state of individual
 * items.
 * Currently used in pqTreeView
 **/
class PQWIDGETS_EXPORT pqCheckableHeaderView : public QHeaderView
{
  Q_OBJECT

public:
    /// Constructor that creates the header view
    pqCheckableHeaderView(Qt::Orientation orientation, QWidget *parent=0);
    virtual ~pqCheckableHeaderView();

    /// Get the checkstate of the header checkbox for the \em section
    QVariant getCheckState(int section);

signals:
    /// This signal is emitted whenever the state of the \em section header
    /// checkbox changes. The new state can be obtained using the \em
    /// getCheckState method.
    /// \sa getCheckState
    void checkStateChanged(int section) const;

protected:
    /// Paint the header section
    /// Depending on whether the top-level items in the model are checkable a
    /// checkbox is painted left-aligned on the header section.
    /// The checkbox is tristate and the state is decided based on the initial
    /// checkstates of model items.
    /// Reimplemented form QHeaderView::paintSection()
    virtual void paintSection(QPainter *painter, const QRect &rect,
      int logicalIndex) const;

    /// Handle mouse press event on the header.
    /// Checks whether the mouse press was in the checkbox.
    /// Clicking on the checkbox triggers the \em checkStateChanged signal
    /// alongwith a boolean to force repaint the checkbox with the new state.
    /// Reimplemented from QWidget::mousePressEvent()
    void mousePressEvent(QMouseEvent *event);

    /// Update the checkstate of all checkable items in the model based on the
    /// checkstate of the header checkbox.
    /// This will undo any individual item checkstate modifications.
    void updateModelCheckState(int section);

private:
    pqCheckableHeaderViewInternal *Internal;
};

#endif //_pqCheckableHeaderView_h
