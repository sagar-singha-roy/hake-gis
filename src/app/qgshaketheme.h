/***************************************************************************
  qgshaketheme.h
  -------------------
  begin                : October 2026
  copyright            : (C) 2026 by Hake Technologies
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSHAKETHEME_H
#define QGSHAKETHEME_H

#include "qgis_app.h"

#include <QString>

/**
 * Hake GeoDesk light/dark theme policy on top of the existing UI theme folders.
 *
 * "Hake Light" and "Hake Night" are the two Hake themes that can be switched live
 * (both run under Fusion). "Hake Dark" is the separate brand-blue theme and is
 * treated like every other non-Hake theme here.
 *
 * The explicit theme stays in UI/UITheme. The appearance mode (System, Light,
 * Dark, Custom) is kept separately in UI/appearanceMode so UI/UITheme always
 * holds a real theme folder name.
 */
class APP_EXPORT QgsHakeTheme
{
  public:
    enum class Variant
    {
      None,  //!< Not a Hake light/dark theme
      Light, //!< Hake Light
      Night, //!< Hake Night
    };

    enum class AppearanceMode
    {
      System, //!< Follow the OS color scheme (Hake Light or Hake Night)
      Light,  //!< Hake Light
      Dark,   //!< Hake Night
      Custom, //!< Any theme chosen in the theme list
    };

    //! Folder name of the Hake light theme.
    static QString lightTheme();

    //! Folder name of the Hake true dark theme.
    static QString nightTheme();

    //! Returns the Hake variant of \a themeName.
    static Variant variantForTheme( const QString &themeName );

    //! Returns TRUE if switching from \a fromTheme to \a toTheme can be applied without a restart.
    static bool isLiveSwitchable( const QString &fromTheme, const QString &toTheme );

    /**
     * Checks that the resources of \a themeName can be loaded before the theme is applied.
     * Returns FALSE and sets \a error if the theme folder or one of its files is missing or unreadable.
     * The built-in "default" theme is always valid.
     */
    static bool validateThemeResources( const QString &themeName, QString *error = nullptr );

    //! Returns the stored appearance mode, derived from UI/UITheme when none was stored yet.
    static AppearanceMode appearanceMode();

    //! Returns the settings value for \a mode.
    static QString appearanceModeToString( AppearanceMode mode );

    //! Returns the mode for the settings value \a value, or Custom for unknown values.
    static AppearanceMode appearanceModeFromString( const QString &value );

    /**
     * Returns the OS color scheme as a Hake variant: Light, Night, or None when the
     * platform or Qt version does not report one.
     */
    static Variant systemVariant();

    /**
     * Returns the theme to use for \a mode. \a fallbackTheme is used for Custom, and for
     * System when the OS does not report a color scheme.
     */
    static QString resolveTheme( AppearanceMode mode, const QString &fallbackTheme );

    /**
     * Stores \a themeName in UI/UITheme and \a mode in UI/appearanceMode.
     * Returns FALSE if the settings could not be written; the caller keeps the
     * in-memory theme regardless.
     */
    static bool persist( const QString &themeName, AppearanceMode mode );
};

#endif // QGSHAKETHEME_H
