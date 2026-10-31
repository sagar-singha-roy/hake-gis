/***************************************************************************
  qgshakebuildinfo.h
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
#ifndef QGSHAKEBUILDINFO_H
#define QGSHAKEBUILDINFO_H

#include "qgis_app.h"

#include <QString>

/**
 * Hake Geospatial code revision: the Git commit this binary was built from.
 *
 * The values are embedded at build time (generated qgsversion.h); nothing is
 * looked up at runtime.
 */
class APP_EXPORT QgsHakeBuildInfo
{
  public:
    //! Full commit SHA, or a placeholder such as "unknown" when the build had no Git revision.
    static QString revision();

    //! First 8 characters of the commit SHA, or the placeholder.
    static QString shortRevision();

    //! Repository URL of the exact commit, or an empty string when the revision is not a commit SHA.
    static QString commitUrl();
};

#endif // QGSHAKEBUILDINFO_H
