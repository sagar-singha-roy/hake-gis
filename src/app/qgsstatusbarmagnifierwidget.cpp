/***************************************************************************
                         qgsstatusbarmagnifierwidget.cpp
    begin                : April 2016
    copyright            : (C) 2016 Paul Blottiere, Oslandia
    email                : paul dot blottiere at oslandia dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsstatusbarmagnifierwidget.h"

#include "qgsapplication.h"
#include "qgsdoublespinbox.h"
#include "qgsguiutils.h"
#include "qgshakeicons.h"
#include "qgssettings.h"
#include "qgssettingsregistrygui.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QToolButton>

#include "moc_qgsstatusbarmagnifierwidget.cpp"

using namespace Qt::StringLiterals;

QgsStatusBarMagnifierWidget::QgsStatusBarMagnifierWidget( QWidget *parent )
  : QWidget( parent )
{
  const QgsSettings settings;
  const int minimumFactor = 100 * QgsGuiUtils::CANVAS_MAGNIFICATION_MIN;
  const int maximumFactor = 100 * QgsGuiUtils::CANVAS_MAGNIFICATION_MAX;
  const int defaultFactor = 100 * QgsSettingsRegistryGui::settingsMagnifierFactorDefault->value();

  // label
  mLabel = new QLabel();
  mLabel->setMinimumWidth( 10 );
  mLabel->setMargin( 3 );
  mLabel->setAlignment( Qt::AlignCenter );
  mLabel->setFrameStyle( QFrame::NoFrame );
  mLabel->setText( tr( "Magnifier" ) );
  mLabel->setToolTip( tr( "Magnifier" ) );

  mSpinBox = new QgsDoubleSpinBox();
  mSpinBox->setSuffix( u"%"_s );
  mSpinBox->setKeyboardTracking( false );
  mSpinBox->setMaximumWidth( 120 );
  mSpinBox->setDecimals( 0 );
  mSpinBox->setRange( minimumFactor, maximumFactor );
  mSpinBox->setWrapping( false );
  mSpinBox->setSingleStep( 50 );
  mSpinBox->setToolTip( tr( "Magnifier level" ) );
  mSpinBox->setClearValueMode( QgsDoubleSpinBox::CustomValue );
  mSpinBox->setClearValue( defaultFactor );

  connect( mSpinBox, static_cast<void ( QgsDoubleSpinBox::* )( double )>( &QgsDoubleSpinBox::valueChanged ), this, &QgsStatusBarMagnifierWidget::setMagnification );

  mLockButton = new QToolButton();
  mLockButton->setIcon( QgsHakeIcons::iconFor( u"statusbar:magnifier-lock"_s, u"/lockedGray.svg"_s ) );
  mLockButton->setIconSize( QSize( 16, 16 ) );
  mLockButton->setToolTip( tr( "Lock the scale to use magnifier to zoom in or out." ) );
  mLockButton->setCheckable( true );
  mLockButton->setChecked( false );
  mLockButton->setAutoRaise( true );

  connect( mLockButton, &QAbstractButton::toggled, this, &QgsStatusBarMagnifierWidget::scaleLockChanged );

  QLabel *magnifierIcon = new QLabel( this );
  magnifierIcon->setObjectName( u"mMagnifierIcon"_s );
  magnifierIcon->setPixmap( QgsHakeIcons::icon( u"navigation/hake-navigation-zoom-in.svg"_s ).pixmap( 16, 16 ) );
  magnifierIcon->setToolTip( tr( "Magnifier" ) );

  // layout
  mLayout = new QHBoxLayout( this );
  mLayout->addWidget( mLockButton );
  mLayout->addWidget( magnifierIcon );
  mLayout->addWidget( mLabel );
  mLayout->addWidget( mSpinBox );
  mLayout->setContentsMargins( 0, 0, 0, 0 );
  mLayout->setAlignment( Qt::AlignRight );
  mLayout->setSpacing( 2 );

  setLayout( mLayout );
}

void QgsStatusBarMagnifierWidget::setDefaultFactor( double factor )
{
  mSpinBox->setClearValue( 100 * factor );
}

void QgsStatusBarMagnifierWidget::setFont( const QFont &myFont )
{
  mLabel->setFont( myFont );
  mSpinBox->setFont( myFont );
}

void QgsStatusBarMagnifierWidget::refreshIcons()
{
  QLabel *iconLabel = findChild<QLabel *>( u"mMagnifierIconLabel"_s );
  if ( iconLabel )
    iconLabel->setPixmap( QgsHakeIcons::icon( u"navigation/hake-navigation-zoom-in.svg"_s ).pixmap( 16, 16 ) );
  mLockButton->setIcon( QgsHakeIcons::iconFor( u"statusbar:magnifier-lock"_s, u"/lockedGray.svg"_s ) );
  mLockButton->setIconSize( QSize( 16, 16 ) );
}

void QgsStatusBarMagnifierWidget::updateMagnification( double factor )
{
  mSpinBox->setValue( factor * 100 );
}

void QgsStatusBarMagnifierWidget::updateScaleLock( bool locked )
{
  mLockButton->setChecked( locked );
}

void QgsStatusBarMagnifierWidget::setMagnification( double value )
{
  emit magnificationChanged( value / 100 );
}
