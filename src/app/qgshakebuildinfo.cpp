/***************************************************************************
  qgshakebuildinfo.cpp
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

#include "qgshakebuildinfo.h"

#include "qgsversion.h"

QString QgsHakeBuildInfo::revision()
{
  return QString::fromLatin1( HAKE_GIT_REVISION );
}

QString QgsHakeBuildInfo::shortRevision()
{
  return QString::fromLatin1( HAKE_GIT_REVISION_SHORT );
}

QString QgsHakeBuildInfo::commitUrl()
{
  return QString::fromLatin1( HAKE_GIT_COMMIT_URL );
}
