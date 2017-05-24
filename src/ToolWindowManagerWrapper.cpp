/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Pavel Strakhov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include "ToolWindowManagerWrapper.h"
#include "ToolWindowManager.h"
#include "ToolWindowManagerArea.h"
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QDebug>
#include <QApplication>

ToolWindowManagerWrapper::ToolWindowManagerWrapper(ToolWindowManager *manager, bool floating) :
  QFrame(manager)
, m_manager(manager)
{
  setWindowFlags(windowFlags() | Qt::Tool | Qt::FramelessWindowHint);
  setWindowTitle(QStringLiteral(" "));

  m_overlay = manager->createDragOverlayWidget();

  m_dragReady = false;
  m_dragActive = false;

  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  m_manager->m_wrappers << this;

  m_titlebar = NULL;

  if (floating) {
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    m_titlebar = new QLabel(QStringLiteral(" "), this);
    m_titlebar->setStyleSheet(QStringLiteral("background-color: #0C266C; color: #ffffff; padding: 2px;"));
    m_titlebar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    mainLayout->addWidget(m_titlebar);
    mainLayout->setMargin(0);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(QMargins(0, 0, 0, 0));

    installEventFilter(this);
    m_titlebar->installEventFilter(this);
  }
}

ToolWindowManagerWrapper::~ToolWindowManagerWrapper() {
  delete m_overlay;
  m_manager->m_wrappers.removeOne(this);
}

void ToolWindowManagerWrapper::closeEvent(QCloseEvent *) {
  QList<QWidget*> toolWindows;
  foreach(ToolWindowManagerArea* tabWidget, findChildren<ToolWindowManagerArea*>()) {
    toolWindows << tabWidget->toolWindows();
  }
  m_manager->moveToolWindows(toolWindows, ToolWindowManager::NoArea);
}

void ToolWindowManagerWrapper::showOverlay() {
  m_overlay->show();
  QRect g = geometry();
  if (!m_titlebar)
    g.moveTopLeft(mapToGlobal(g.topLeft()));
  m_overlay->setGeometry(g);
}

void ToolWindowManagerWrapper::hideOverlay() {
  m_overlay->hide();
}

void ToolWindowManagerWrapper::finishDragMove() {
  move(m_dragStartPos - m_dragStartCursor + QCursor::pos());
}

QRect ToolWindowManagerWrapper::dragGeometry() {
  return QRect(m_dragStartPos - m_dragStartCursor + QCursor::pos(), size());
}

bool ToolWindowManagerWrapper::eventFilter(QObject *object, QEvent *event) {
  if (object == this) {
    if (event->type() == QEvent::MouseButtonRelease) {
      // if the mouse button is released, let the manager finish the drag and don't call any more
      // updates for any further move events
      m_dragActive = false;
      m_manager->updateDragPosition();
    } else if (event->type() == QEvent::MouseMove) {
      // if we're ready to start a drag, check how far we've moved and start the drag if past a
      // certain pixel threshold.
      if (m_dragReady) {
        if ((QCursor::pos() - m_dragStartCursor).manhattanLength() > 10) {
          m_dragActive = true;
          m_dragReady = false;
          QList<QWidget*> toolWindows;
          foreach(ToolWindowManagerArea* tabWidget, findChildren<ToolWindowManagerArea*>()) {
            toolWindows << tabWidget->toolWindows();
          }
          m_manager->startDrag(toolWindows, this);
        }
      }
      // if the drag is active, update it in the manager.
      if (m_dragActive) {
        m_manager->updateDragPosition();
      }
    }
  } else if (object == m_titlebar) {
    // if we get a click on the titlebar, indicate that a drag is possible once we've moved a bit.
    if(event->type() == QEvent::MouseButtonPress) {
      m_dragReady = true;
      m_dragStartCursor = QCursor::pos();
      m_dragStartPos = pos();
    }
  }
  return QFrame::eventFilter(object, event);
}

QVariantMap ToolWindowManagerWrapper::saveState() {
  if (layout()->count() > 2) {
    qWarning("too many children for wrapper");
    return QVariantMap();
  }
  if (isWindow() && layout()->count() == 0) {
    qWarning("empty top level wrapper");
    return QVariantMap();
  }
  QVariantMap result;
  result[QStringLiteral("geometry")] = saveGeometry().toBase64();
  QSplitter* splitter = findChild<QSplitter*>(QString(), Qt::FindDirectChildrenOnly);
  if (splitter) {
    result[QStringLiteral("splitter")] = m_manager->saveSplitterState(splitter);
  } else {
    ToolWindowManagerArea* area = findChild<ToolWindowManagerArea*>();
    if (area) {
      result[QStringLiteral("area")] = area->saveState();
    } else if (layout()->count() > 0) {
      qWarning("unknown child");
      return QVariantMap();
    }
  }
  return result;
}

void ToolWindowManagerWrapper::restoreState(const QVariantMap &savedData) {
  restoreGeometry(QByteArray::fromBase64(savedData[QStringLiteral("geometry")].toByteArray()));
  if (layout()->count() > 1) {
    qWarning("wrapper is not empty");
    return;
  }
  if (savedData.contains(QStringLiteral("splitter"))) {
    layout()->addWidget(m_manager->restoreSplitterState(savedData[QStringLiteral("splitter")].toMap()));
  } else if (savedData.contains(QStringLiteral("area"))) {
    ToolWindowManagerArea* area = m_manager->createArea();
    area->restoreState(savedData[QStringLiteral("area")].toMap());
    layout()->addWidget(area);
  }
}
