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
#include "ToolWindowManager.h"
#include "ToolWindowManagerArea.h"
#include "ToolWindowManagerWrapper.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QEvent>
#include <QApplication>
#include <QDrag>
#include <QMetaMethod>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QDesktopWidget>
#include <QScreen>
#include <QSplitter>
#include <QTabBar>

template<class T>
T findClosestParent(QWidget* widget) {
  while(widget) {
    if (qobject_cast<T>(widget)) {
      return static_cast<T>(widget);
    }
    widget = widget->parentWidget();
  }
  return 0;
}

ToolWindowManager::ToolWindowManager(QWidget *parent) :
  QWidget(parent)
{
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  ToolWindowManagerWrapper* wrapper = new ToolWindowManagerWrapper(this, false);
  wrapper->setWindowFlags(wrapper->windowFlags() & ~Qt::Tool);
  mainLayout->addWidget(wrapper);
  m_allowFloatingWindow = true;
  m_createCallback = NULL;
  m_lastUsedArea = NULL;

  m_draggedWrapper = NULL;
  m_hoverArea = NULL;

  m_previewOverlay = new QWidget(NULL);

  QPalette pal = palette();
  pal.setColor(QPalette::Background, pal.color(QPalette::Highlight));
  m_previewOverlay->setAutoFillBackground(true);
  m_previewOverlay->setPalette(pal);
  m_previewOverlay->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  m_previewOverlay->setWindowOpacity(0.3);
  m_previewOverlay->setAttribute(Qt::WA_ShowWithoutActivating);
  m_previewOverlay->hide();

  m_dropHotspotsOverlay = new QWidget(NULL);

  m_dropHotspotsOverlay->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  m_dropHotspotsOverlay->setAttribute(Qt::WA_NoSystemBackground);
  m_dropHotspotsOverlay->setAttribute(Qt::WA_TranslucentBackground);
  m_dropHotspotsOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
  m_dropHotspotsOverlay->setAttribute(Qt::WA_ShowWithoutActivating);
  m_dropHotspotsOverlay->hide();

  for (int i=0; i < NumReferenceTypes; i++)
    m_dropHotspots[i] = NULL;

  // TODO make configurable
  const int dropHotspotSize = 32;

  // TODO generate proper pixmaps for these (and then allow them to be set externally).
  m_dropHotspots[AddTo] = new QLabel(m_dropHotspotsOverlay);
  m_dropHotspots[AddTo]->setStyleSheet(QStringLiteral("background-color: #ff0000;"));
  m_dropHotspots[AddTo]->setFixedSize(dropHotspotSize, dropHotspotSize);

  m_dropHotspots[TopOf] = new QLabel(m_dropHotspotsOverlay);
  m_dropHotspots[TopOf]->setStyleSheet(QStringLiteral("background-color: #00ff00;"));
  m_dropHotspots[TopOf]->setFixedSize(dropHotspotSize, dropHotspotSize);

  m_dropHotspots[LeftOf] = new QLabel(m_dropHotspotsOverlay);
  m_dropHotspots[LeftOf]->setStyleSheet(QStringLiteral("background-color: #0000ff;"));
  m_dropHotspots[LeftOf]->setFixedSize(dropHotspotSize, dropHotspotSize);

  m_dropHotspots[RightOf] = new QLabel(m_dropHotspotsOverlay);
  m_dropHotspots[RightOf]->setStyleSheet(QStringLiteral("background-color: #ff00ff;"));
  m_dropHotspots[RightOf]->setFixedSize(dropHotspotSize, dropHotspotSize);

  m_dropHotspots[BottomOf] = new QLabel(m_dropHotspotsOverlay);
  m_dropHotspots[BottomOf]->setStyleSheet(QStringLiteral("background-color: #00ffff;"));
  m_dropHotspots[BottomOf]->setFixedSize(dropHotspotSize, dropHotspotSize);

  m_dropHotspots[TopWindowSide] = new QLabel(m_dropHotspotsOverlay);
  m_dropHotspots[TopWindowSide]->setStyleSheet(QStringLiteral("background-color: #ffff00;"));
  m_dropHotspots[TopWindowSide]->setFixedSize(dropHotspotSize, dropHotspotSize);

  m_dropHotspots[LeftWindowSide] = new QLabel(m_dropHotspotsOverlay);
  m_dropHotspots[LeftWindowSide]->setStyleSheet(QStringLiteral("background-color: #808080;"));
  m_dropHotspots[LeftWindowSide]->setFixedSize(dropHotspotSize, dropHotspotSize);

  m_dropHotspots[RightWindowSide] = new QLabel(m_dropHotspotsOverlay);
  m_dropHotspots[RightWindowSide]->setStyleSheet(QStringLiteral("background-color: #000000;"));
  m_dropHotspots[RightWindowSide]->setFixedSize(dropHotspotSize, dropHotspotSize);

  m_dropHotspots[BottomWindowSide] = new QLabel(m_dropHotspotsOverlay);
  m_dropHotspots[BottomWindowSide]->setStyleSheet(QStringLiteral("background-color: #dd6600;"));
  m_dropHotspots[BottomWindowSide]->setFixedSize(dropHotspotSize, dropHotspotSize);
}

ToolWindowManager::~ToolWindowManager() {
  delete m_previewOverlay;
  delete m_dropHotspotsOverlay;
  while(!m_areas.isEmpty()) {
    delete m_areas.first();
  }
  while(!m_wrappers.isEmpty()) {
    delete m_wrappers.first();
  }
}

void ToolWindowManager::setToolWindowProperties(QWidget* toolWindow, ToolWindowManager::ToolWindowProperty properties) {
  m_toolWindowProperties[toolWindow] = properties;
  ToolWindowManagerArea *area = areaOf(toolWindow);
  if(area)
    area->updateToolWindow(toolWindow);
}

ToolWindowManager::ToolWindowProperty ToolWindowManager::toolWindowProperties(QWidget* toolWindow) {
  return m_toolWindowProperties[toolWindow];
}

void ToolWindowManager::addToolWindow(QWidget *toolWindow, const AreaReference &area) {
  addToolWindows(QList<QWidget*>() << toolWindow, area);
}

void ToolWindowManager::addToolWindows(QList<QWidget *> toolWindows, const ToolWindowManager::AreaReference &area) {
  foreach(QWidget* toolWindow, toolWindows) {
    if (!toolWindow) {
      qWarning("cannot add null widget");
      continue;
    }
    if (m_toolWindows.contains(toolWindow)) {
      qWarning("this tool window has already been added");
      continue;
    }
    toolWindow->hide();
    toolWindow->setParent(0);
    m_toolWindows << toolWindow;
    m_toolWindowProperties[toolWindow] = ToolWindowProperty(0);
    QObject::connect(toolWindow, &QWidget::windowTitleChanged, this, &ToolWindowManager::windowTitleChanged);
  }
  moveToolWindows(toolWindows, area);
}

ToolWindowManagerArea *ToolWindowManager::areaOf(QWidget *toolWindow) {
  return findClosestParent<ToolWindowManagerArea*>(toolWindow);
}

void ToolWindowManager::moveToolWindow(QWidget *toolWindow, AreaReference area) {
  moveToolWindows(QList<QWidget*>() << toolWindow, area);
}

void ToolWindowManager::moveToolWindows(QList<QWidget *> toolWindows,
                                        ToolWindowManager::AreaReference area) {
  foreach(QWidget* toolWindow, toolWindows) {
    if (!m_toolWindows.contains(toolWindow)) {
      qWarning("unknown tool window");
      return;
    }
    if (toolWindow->parentWidget() != 0) {
      releaseToolWindow(toolWindow);
    }
  }
  if (area.type() == LastUsedArea && !m_lastUsedArea) {
    ToolWindowManagerArea* foundArea = findChild<ToolWindowManagerArea*>();
    if (foundArea) {
      area = AreaReference(AddTo, foundArea);
    } else {
      area = EmptySpace;
    }
  }

  if (area.type() == NoArea) {
    //do nothing
  } else if (area.type() == NewFloatingArea) {
    ToolWindowManagerArea* floatArea = createArea();
    floatArea->addToolWindows(toolWindows);
    ToolWindowManagerWrapper* wrapper = new ToolWindowManagerWrapper(this, true);
    wrapper->layout()->addWidget(floatArea);
    wrapper->move(QCursor::pos());
    wrapper->show();
  } else if (area.type() == AddTo) {
    area.area()->addToolWindows(toolWindows);
  } else if (area.type() == LeftWindowSide || area.type() == RightWindowSide ||
             area.type() == TopWindowSide || area.type() == BottomWindowSide) {
    ToolWindowManagerWrapper* wrapper = findClosestParent<ToolWindowManagerWrapper*>(area.area());
    if (!wrapper) {
      qWarning("couldn't find wrapper");
      return;
    }

    if (wrapper->layout()->count() > 1)
    {
      qWarning("wrapper has multiple direct children");
      return;
    }

    QLayoutItem* item = wrapper->layout()->takeAt(0);

    QSplitter* splitter = createSplitter();
    if (area.type() == TopWindowSide || area.type() == BottomWindowSide) {
      splitter->setOrientation(Qt::Vertical);
    } else {
      splitter->setOrientation(Qt::Horizontal);
    }

    splitter->addWidget(item->widget());
    area.widget()->show();

    delete item;

    ToolWindowManagerArea* newArea = createArea();
    newArea->addToolWindows(toolWindows);

    if (area.type() == TopWindowSide || area.type() == LeftWindowSide) {
      splitter->insertWidget(0, newArea);
    } else {
      splitter->addWidget(newArea);
    }

    wrapper->layout()->addWidget(splitter);
  } else if (area.type() == LeftOf || area.type() == RightOf ||
             area.type() == TopOf || area.type() == BottomOf) {
    QSplitter* parentSplitter = qobject_cast<QSplitter*>(area.widget()->parentWidget());
    ToolWindowManagerWrapper* wrapper = qobject_cast<ToolWindowManagerWrapper*>(area.widget()->parentWidget());
    if (!parentSplitter && !wrapper) {
      qWarning("unknown parent type");
      return;
    }
    bool useParentSplitter = false;
    int indexInParentSplitter = 0;
    QList<QRect> parentSplitterGeometries;
    QList<int> parentSplitterSizes;
    if (parentSplitter) {
      indexInParentSplitter = parentSplitter->indexOf(area.widget());
      parentSplitterSizes = parentSplitter->sizes();
      for(int i = 0; i < parentSplitter->count(); i++) {
        parentSplitterGeometries.push_back(parentSplitter->widget(i)->geometry());
      }
      if (parentSplitter->orientation() == Qt::Vertical) {
        useParentSplitter = area.type() == TopOf || area.type() == BottomOf;
      } else {
        useParentSplitter = area.type() == LeftOf || area.type() == RightOf;
      }
    }
    if (useParentSplitter) {
      if (area.type() == BottomOf || area.type() == RightOf) {
        indexInParentSplitter++;
      }
      ToolWindowManagerArea* newArea = createArea();
      newArea->addToolWindows(toolWindows);
      parentSplitter->insertWidget(indexInParentSplitter, newArea);

      for(int i = 0; i < qMin(parentSplitter->count(), parentSplitterGeometries.count()); i++) {
        parentSplitter->widget(i)->setGeometry(parentSplitterGeometries[i]);
      }
    } else {
      area.widget()->hide();
      area.widget()->setParent(0);
      QSplitter* splitter = createSplitter();
      if (area.type() == TopOf || area.type() == BottomOf) {
        splitter->setOrientation(Qt::Vertical);
      } else {
        splitter->setOrientation(Qt::Horizontal);
      }

      ToolWindowManagerArea* newArea = createArea();

      // inherit the size policy from the widget we are wrapping
      splitter->setSizePolicy(area.widget()->sizePolicy());

      // store old geometries so we can restore them
      QRect areaGeometry = area.widget()->geometry();
      QRect newGeometry = newArea->geometry();

      splitter->addWidget(area.widget());
      area.widget()->show();

      int indexInSplitter = 0;

      if (area.type() == TopOf || area.type() == LeftOf) {
        splitter->insertWidget(0, newArea);
        indexInSplitter = 0;
      } else {
        splitter->addWidget(newArea);
        indexInSplitter = 1;
      }

      // Convert area percentage desired to a stretch factor.
      const int totalStretch = 100;
      int pct = int(totalStretch*area.percentage());
      splitter->setStretchFactor(indexInSplitter, pct);
      splitter->setStretchFactor(1-indexInSplitter, totalStretch-pct);

      if (parentSplitter) {
        parentSplitter->insertWidget(indexInParentSplitter, splitter);

        for (int i = 0; i < qMin(parentSplitter->count(), parentSplitterGeometries.count()); i++) {
          parentSplitter->widget(i)->setGeometry(parentSplitterGeometries[i]);
        }

        if (parentSplitterSizes.count() > 0 && parentSplitterSizes[0] != 0) {
          parentSplitter->setSizes(parentSplitterSizes);
        }

      } else {
        wrapper->layout()->addWidget(splitter);
      }

      newArea->addToolWindows(toolWindows);

      area.widget()->setGeometry(areaGeometry);
      newArea->setGeometry(newGeometry);
    }
  } else if (area.type() == EmptySpace) {
    ToolWindowManagerArea* newArea = createArea();
    findChild<ToolWindowManagerWrapper*>()->layout()->addWidget(newArea);
    newArea->addToolWindows(toolWindows);
  } else if (area.type() == LastUsedArea) {
    m_lastUsedArea->addToolWindows(toolWindows);
  } else {
    qWarning("invalid type");
  }
  simplifyLayout();
  foreach(QWidget* toolWindow, toolWindows) {
    emit toolWindowVisibilityChanged(toolWindow, toolWindow->parent() != 0);
  }
}

void ToolWindowManager::removeToolWindow(QWidget *toolWindow) {
  if (!m_toolWindows.contains(toolWindow)) {
    qWarning("unknown tool window");
    return;
  }
  moveToolWindow(toolWindow, NoArea);
  m_toolWindows.removeOne(toolWindow);
  m_toolWindowProperties.remove(toolWindow);
  delete toolWindow;
}

ToolWindowManager* ToolWindowManager::managerOf(QWidget* toolWindow) {
  if (!toolWindow) {
    qWarning("NULL tool window");
    return NULL;
  }

  return findClosestParent<ToolWindowManager*>(toolWindow);
}

void ToolWindowManager::closeToolWindow(QWidget *toolWindow) {
  if (!toolWindow) {
    qWarning("NULL tool window");
    return;
  }

  // search up to find the first parent manager
  ToolWindowManager *manager = findClosestParent<ToolWindowManager*>(toolWindow);

  if(manager) {
    manager->removeToolWindow(toolWindow);
    return;
  }

  qWarning("window not child of any tool window");
}

void ToolWindowManager::raiseToolWindow(QWidget *toolWindow) {
  if (!toolWindow) {
    qWarning("NULL tool window");
    return;
  }

  // if the parent is a ToolWindowManagerArea, switch tabs
  QWidget *parent = toolWindow->parentWidget();
  ToolWindowManagerArea *area = qobject_cast<ToolWindowManagerArea*>(parent);
  if(area == NULL)
    parent = parent->parentWidget();

  area = qobject_cast<ToolWindowManagerArea*>(parent);

  if(area)
    area->setCurrentWidget(toolWindow);
  else
    qWarning("parent is not a tool window area");
}

QWidget* ToolWindowManager::createToolWindow(const QString& objectName)
{
  if (m_createCallback) {
    QWidget *toolWindow = m_createCallback(objectName);
    if(toolWindow) {
      m_toolWindows << toolWindow;
      m_toolWindowProperties[toolWindow] = ToolWindowProperty(0);
      QObject::connect(toolWindow, &QWidget::windowTitleChanged, this, &ToolWindowManager::windowTitleChanged);
      return toolWindow;
    }
  }

  return NULL;
}

void ToolWindowManager::setAllowFloatingWindow(bool allow) {
  m_allowFloatingWindow = allow;
}

QVariantMap ToolWindowManager::saveState() {
  QVariantMap result;
  result[QStringLiteral("toolWindowManagerStateFormat")] = 1;
  ToolWindowManagerWrapper* mainWrapper = findChild<ToolWindowManagerWrapper*>();
  if (!mainWrapper) {
    qWarning("can't find main wrapper");
    return QVariantMap();
  }
  result[QStringLiteral("mainWrapper")] = mainWrapper->saveState();
  QVariantList floatingWindowsData;
  foreach(ToolWindowManagerWrapper* wrapper, m_wrappers) {
    if (!wrapper->isWindow()) { continue; }
    floatingWindowsData << wrapper->saveState();
  }
  result[QStringLiteral("floatingWindows")] = floatingWindowsData;
  return result;
}

void ToolWindowManager::restoreState(const QVariantMap &dataMap) {
  if (dataMap.isEmpty()) { return; }
  if (dataMap[QStringLiteral("toolWindowManagerStateFormat")].toInt() != 1) {
    qWarning("state format is not recognized");
    return;
  }
  moveToolWindows(m_toolWindows, NoArea);
  ToolWindowManagerWrapper* mainWrapper = findChild<ToolWindowManagerWrapper*>();
  if (!mainWrapper) {
    qWarning("can't find main wrapper");
    return;
  }
  mainWrapper->restoreState(dataMap[QStringLiteral("mainWrapper")].toMap());
  QVariantList floatWins = dataMap[QStringLiteral("floatingWindows")].toList();
  foreach(QVariant windowData, floatWins) {
    ToolWindowManagerWrapper* wrapper = new ToolWindowManagerWrapper(this, true);
    wrapper->restoreState(windowData.toMap());
    wrapper->show();
    if(wrapper->windowState() && Qt::WindowMaximized)
    {
      wrapper->setWindowState(0);
      wrapper->setWindowState(Qt::WindowMaximized);
    }
  }
  simplifyLayout();
  foreach(QWidget* toolWindow, m_toolWindows) {
    emit toolWindowVisibilityChanged(toolWindow, toolWindow->parentWidget() != 0);
  }
}

ToolWindowManagerArea *ToolWindowManager::createArea() {
  ToolWindowManagerArea* area = new ToolWindowManagerArea(this, 0);
  connect(area, SIGNAL(tabCloseRequested(int)),
          this, SLOT(tabCloseRequested(int)));
  return area;
}

void ToolWindowManager::releaseToolWindow(QWidget *toolWindow) {
  ToolWindowManagerArea* previousTabWidget = findClosestParent<ToolWindowManagerArea*>(toolWindow);
  if (!previousTabWidget) {
    qWarning("cannot find tab widget for tool window");
    return;
  }
  previousTabWidget->removeTab(previousTabWidget->indexOf(toolWindow));
  toolWindow->hide();
  toolWindow->setParent(0);

}

void ToolWindowManager::simplifyLayout() {
  foreach(ToolWindowManagerArea* area, m_areas) {
    if (area->parentWidget() == 0) {
      if (area->count() == 0) {
        if (area == m_lastUsedArea) { m_lastUsedArea = 0; }
        //QTimer::singleShot(1000, area, SLOT(deleteLater()));
        area->deleteLater();
      }
      continue;
    }
    QSplitter* splitter = qobject_cast<QSplitter*>(area->parentWidget());
    QSplitter* validSplitter = 0; // least top level splitter that should remain
    QSplitter* invalidSplitter = 0; //most top level splitter that should be deleted
    while(splitter) {
      if (splitter->count() > 1) {
        validSplitter = splitter;
        break;
      } else {
        invalidSplitter = splitter;
        splitter = qobject_cast<QSplitter*>(splitter->parentWidget());
      }
    }
    if (!validSplitter) {
      ToolWindowManagerWrapper* wrapper = findClosestParent<ToolWindowManagerWrapper*>(area);
      if (!wrapper) {
        qWarning("can't find wrapper");
        return;
      }
      if (area->count() == 0 && wrapper->isWindow()) {
        wrapper->hide();
        // can't deleteLater immediately (strange MacOS bug)
        //QTimer::singleShot(1000, wrapper, SLOT(deleteLater()));
        wrapper->deleteLater();
      } else if (area->parent() != wrapper) {
        wrapper->layout()->addWidget(area);
      }
    } else {
      if (area->count() > 0) {
        if (validSplitter && area->parent() != validSplitter) {
          int index = validSplitter->indexOf(invalidSplitter);
          validSplitter->insertWidget(index, area);
        }
      }
    }
    if (invalidSplitter) {
      invalidSplitter->hide();
      invalidSplitter->setParent(0);
      //QTimer::singleShot(1000, invalidSplitter, SLOT(deleteLater()));
      invalidSplitter->deleteLater();
    }
    if (area->count() == 0) {
      area->hide();
      area->setParent(0);
      if (area == m_lastUsedArea) { m_lastUsedArea = 0; }
      //QTimer::singleShot(1000, area, SLOT(deleteLater()));
      area->deleteLater();
    }
  }
}

void ToolWindowManager::startDrag(const QList<QWidget *> &toolWindows,
                                  ToolWindowManagerWrapper *wrapper) {
  if (dragInProgress()) {
    qWarning("ToolWindowManager::execDrag: drag is already in progress");
    return;
  }
  foreach(QWidget* toolWindow, toolWindows) {
    if(toolWindowProperties(toolWindow) & DisallowUserDocking) { return; }
  }
  if (toolWindows.isEmpty()) { return; }

  m_draggedWrapper = wrapper;
  m_draggedToolWindows = toolWindows;
  updateDragPosition();
  qApp->installEventFilter(this);
}

QVariantMap ToolWindowManager::saveSplitterState(QSplitter *splitter) {
  QVariantMap result;
  result[QStringLiteral("state")] = splitter->saveState().toBase64();
  result[QStringLiteral("type")] = QStringLiteral("splitter");
  QVariantList items;
  for(int i = 0; i < splitter->count(); i++) {
    QWidget* item = splitter->widget(i);
    QVariantMap itemValue;
    ToolWindowManagerArea* area = qobject_cast<ToolWindowManagerArea*>(item);
    if (area) {
      itemValue = area->saveState();
    } else {
      QSplitter* childSplitter = qobject_cast<QSplitter*>(item);
      if (childSplitter) {
        itemValue = saveSplitterState(childSplitter);
      } else {
        qWarning("unknown splitter item");
      }
    }
    items << itemValue;
  }
  result[QStringLiteral("items")] = items;
  return result;
}

QSplitter *ToolWindowManager::restoreSplitterState(const QVariantMap &savedData) {
  if (savedData[QStringLiteral("items")].toList().count() < 2) {
    qWarning("invalid splitter encountered");
  }
  QSplitter* splitter = createSplitter();

  QVariantList itemList = savedData[QStringLiteral("items")].toList();
  foreach(QVariant itemData, itemList) {
    QVariantMap itemValue = itemData.toMap();
    QString itemType = itemValue[QStringLiteral("type")].toString();
    if (itemType == QStringLiteral("splitter")) {
      splitter->addWidget(restoreSplitterState(itemValue));
    } else if (itemType == QStringLiteral("area")) {
      ToolWindowManagerArea* area = createArea();
      area->restoreState(itemValue);
      splitter->addWidget(area);
    } else {
      qWarning("unknown item type");
    }
  }
  splitter->restoreState(QByteArray::fromBase64(savedData[QStringLiteral("state")].toByteArray()));
  return splitter;
}

void ToolWindowManager::updateDragPosition() {
  if (!dragInProgress()) { return; }
  if (!(qApp->mouseButtons() & Qt::LeftButton)) {
    finishDrag();
    return;
  }

  QPoint pos = QCursor::pos();
  m_hoverArea = NULL;

  foreach(ToolWindowManagerArea* area, m_areas) {
    // don't allow dragging a whole wrapper into a subset of itself
    if (m_draggedWrapper && area->window() == m_draggedWrapper->window()) {
      continue;
    }
    if (area->rect().contains(area->mapFromGlobal(pos))) {
      m_hoverArea = area;
      break;
    }
  }
  if (m_hoverArea) {
    ToolWindowManagerWrapper* wrapper = findClosestParent<ToolWindowManagerWrapper*>(m_hoverArea);
    QRect wrapperGeometry;
    wrapperGeometry.setSize(wrapper->rect().size());
    wrapperGeometry.moveTo(wrapper->mapToGlobal(QPoint(0,0)));
    m_dropHotspotsOverlay->setGeometry(wrapperGeometry);

    QRect areaClientRect;

    // calculate the rect of the area relative to m_dropHotspotsOverlay
    areaClientRect.setTopLeft(m_hoverArea->mapToGlobal(QPoint(0,0)) - m_dropHotspotsOverlay->pos());
    areaClientRect.setSize(m_hoverArea->rect().size());

    // subtract the rect for the tab bar.
    areaClientRect.adjust(0, m_hoverArea->tabBar()->rect().height(), 0, 0);

    // TODO make configurable externally
    const int margin = 4;

    // TODO fetch this from the external configuration variable, not from the widget itself
    int size = m_dropHotspots[AddTo]->size().width();
    int hsize = size / 2;

    QPoint c = areaClientRect.center();

    m_dropHotspots[AddTo]->move(c + QPoint(-hsize, -hsize));
    m_dropHotspots[AddTo]->show();

    m_dropHotspots[TopOf]->move(c + QPoint(-hsize, -hsize-margin-size));
    m_dropHotspots[TopOf]->show();

    m_dropHotspots[LeftOf]->move(c + QPoint(-hsize-margin-size, -hsize));
    m_dropHotspots[LeftOf]->show();

    m_dropHotspots[RightOf]->move(c + QPoint( hsize+margin, -hsize));
    m_dropHotspots[RightOf]->show();

    m_dropHotspots[BottomOf]->move(c + QPoint(-hsize, hsize+margin));
    m_dropHotspots[BottomOf]->show();

    QRect wrapperClientRect = m_dropHotspotsOverlay->rect();
    c = wrapperClientRect.center();
    QSize s = wrapperClientRect.size();

    m_dropHotspots[TopWindowSide]->move(QPoint(c.x() - hsize, margin * 2));
    m_dropHotspots[TopWindowSide]->show();

    m_dropHotspots[LeftWindowSide]->move(QPoint(margin * 2, c.y() - hsize));
    m_dropHotspots[LeftWindowSide]->show();

    m_dropHotspots[RightWindowSide]->move(QPoint(s.width() - size - margin * 2, c.y() - hsize));
    m_dropHotspots[RightWindowSide]->show();

    m_dropHotspots[BottomWindowSide]->move(QPoint(c.x() - hsize, s.height() - size - margin * 2));
    m_dropHotspots[BottomWindowSide]->show();

    m_dropHotspotsOverlay->show();
  } else {
    m_dropHotspotsOverlay->hide();
  }

  AreaReferenceType hotspot = currentHotspot();
  if (hotspot == AddTo ||
      hotspot == LeftOf || hotspot == RightOf ||
      hotspot == TopOf || hotspot == BottomOf) {
    QRect g = m_hoverArea->geometry();
    g.moveTopLeft(m_hoverArea->parentWidget()->mapToGlobal(g.topLeft()));

    if(hotspot == LeftOf)
      g.adjust(0, 0, -g.width()/2, 0);
    else if(hotspot == RightOf)
      g.adjust(g.width()/2, 0, 0, 0);
    else if(hotspot == TopOf)
      g.adjust(0, 0, 0, -g.height()/2);
    else if(hotspot == BottomOf)
      g.adjust(0, g.height()/2, 0, 0);

    m_previewOverlay->setGeometry(g);
  } else {
    // no hotspot highlighted, draw geometry for a float window
    if (m_draggedWrapper) {
      m_previewOverlay->setGeometry(m_draggedWrapper->dragGeometry());
    } else {
      QRect r;
      for (QWidget *w : m_draggedToolWindows)
        r = r.united(w->rect());
      m_previewOverlay->setGeometry(pos.x(), pos.y(), r.width(), r.height());
    }
  }

  m_previewOverlay->show();
  if (m_dropHotspotsOverlay->isVisible())
    m_dropHotspotsOverlay->raise();
}

void ToolWindowManager::abortDrag() {
  if (!dragInProgress())
    return;

  m_previewOverlay->hide();
  m_dropHotspotsOverlay->hide();
  m_draggedToolWindows.clear();
  m_draggedWrapper = NULL;
  qApp->removeEventFilter(this);
}

void ToolWindowManager::finishDrag() {
  if (!dragInProgress()) {
    qWarning("unexpected finishDrag");
    return;
  }
  qApp->removeEventFilter(this);
  m_previewOverlay->hide();
  m_dropHotspotsOverlay->hide();

  AreaReferenceType hotspot = currentHotspot();

  if (hotspot == NewFloatingArea) {
    // check if we're dragging a whole float window, if so we don't do anything except move it.
    if (m_draggedWrapper) {
      m_draggedWrapper->finishDragMove();
    } else {
      bool allowFloat = m_allowFloatingWindow;

      for (QWidget *w : m_draggedToolWindows)
        allowFloat &= !(toolWindowProperties(w) & DisallowFloatWindow);

      if (m_allowFloatingWindow)
      {
        QRect r;
        for(QWidget *w : m_draggedToolWindows)
          r = r.united(w->rect());

        moveToolWindows(m_draggedToolWindows, NewFloatingArea);

        ToolWindowManagerArea *area = areaOf(m_draggedToolWindows[0]);

        area->parentWidget()->resize(r.size());
      }
    }
  } else {
    moveToolWindows(m_draggedToolWindows, AreaReference(hotspot, m_hoverArea));
  }

  m_draggedToolWindows.clear();
}

ToolWindowManager::AreaReferenceType ToolWindowManager::currentHotspot() {
  QPoint pos = m_dropHotspotsOverlay->mapFromGlobal(QCursor::pos());

  if (m_hoverArea == NULL)
    return NewFloatingArea;

  for (int i=0; i < NumReferenceTypes; i++) {
    if (m_dropHotspots[i] && m_dropHotspots[i]->geometry().contains(pos)) {
      return (ToolWindowManager::AreaReferenceType)i;
    }
  }

  return NewFloatingArea;
}

bool ToolWindowManager::eventFilter(QObject *object, QEvent *event) {
  if (event->type() == QEvent::MouseButtonRelease) {
    // right clicking aborts any drag in progress
    if (static_cast<QMouseEvent*>(event)->button() == Qt::RightButton)
      abortDrag();
  } else if (event->type() == QEvent::KeyPress) {
    // pressing escape any drag in progress
    QKeyEvent *ke = (QKeyEvent *)event;
    if(ke->key() == Qt::Key_Escape) {
      abortDrag();
    }
  }
  return QWidget::eventFilter(object, event);
}

void ToolWindowManager::tabCloseRequested(int index) {
  ToolWindowManagerArea* tabWidget = qobject_cast<ToolWindowManagerArea*>(sender());
  if (!tabWidget) {
    qWarning("sender is not a ToolWindowManagerArea");
    return;
  }
  QWidget* toolWindow = tabWidget->widget(index);
  if (!m_toolWindows.contains(toolWindow)) {
    qWarning("unknown tab in tab widget");
    return;
  }

  int methodIndex = toolWindow->metaObject()->indexOfMethod(QMetaObject::normalizedSignature("checkAllowClose()"));

  if(methodIndex >= 0) {
    bool ret = true;
    toolWindow->metaObject()->method(methodIndex).invoke(toolWindow, Qt::DirectConnection, Q_RETURN_ARG(bool, ret));

    if(!ret)
      return;
  }

  if(toolWindowProperties(toolWindow) & ToolWindowManager::HideOnClose)
    hideToolWindow(toolWindow);
  else
    removeToolWindow(toolWindow);
}

void ToolWindowManager::windowTitleChanged(const QString &) {
  QWidget* toolWindow = qobject_cast<QWidget*>(sender());
  if(!toolWindow) {
    return;
  }
  ToolWindowManagerArea *area = areaOf(toolWindow);
  if(area) {
    area->updateToolWindow(toolWindow);
  }
}

QSplitter *ToolWindowManager::createSplitter() {
  QSplitter* splitter = new QSplitter();
  splitter->setChildrenCollapsible(false);
  return splitter;
}

ToolWindowManager::AreaReference::AreaReference(ToolWindowManager::AreaReferenceType type, ToolWindowManagerArea *area, float percentage) {
  m_type = type;
  m_percentage = percentage;
  setWidget(area);
}

void ToolWindowManager::AreaReference::setWidget(QWidget *widget) {
  if (m_type == LastUsedArea || m_type == NewFloatingArea || m_type == NoArea || m_type == EmptySpace) {
    if (widget != 0) {
      qWarning("area parameter ignored for this type");
    }
    m_widget = 0;
  } else if (m_type == AddTo) {
    m_widget = qobject_cast<ToolWindowManagerArea*>(widget);
    if (!m_widget) {
      qWarning("only ToolWindowManagerArea can be used with this type");
    }
  } else {
    if (!qobject_cast<ToolWindowManagerArea*>(widget) &&
        !qobject_cast<QSplitter*>(widget)) {
      qWarning("only ToolWindowManagerArea or splitter can be used with this type");
      m_widget = 0;
    } else {
      m_widget = widget;
    }
  }
}

ToolWindowManagerArea *ToolWindowManager::AreaReference::area() const {
  return qobject_cast<ToolWindowManagerArea*>(m_widget);
}

ToolWindowManager::AreaReference::AreaReference(ToolWindowManager::AreaReferenceType type, QWidget *widget) {
  m_type = type;
  setWidget(widget);
}
