/***************************************************************************
    mainwin.cpp
    ---------------------
    begin                : February 2017
    copyright            : (C) 2017 by Juergen E. Fischer
    email                : jef at norbit dot de
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <cstdio>
#include <cstdlib>
#include <io.h>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>

namespace
{

void showError( const std::wstring &message, const std::wstring &title )
{
  const std::wstring newmessage = L"Oops, looks like an error loading Hake GIS \n\n Details: \n\n" + message;
  MessageBoxW( nullptr, newmessage.c_str(), title.c_str(), MB_ICONERROR | MB_OK );
  std::wcerr << message << std::endl;
}

std::wstring moduleExePath()
{
  DWORD l = MAX_PATH;
  std::unique_ptr<wchar_t[]> filepath;
  for ( ;; )
  {
    filepath.reset( new wchar_t[l] );
    const DWORD copied = GetModuleFileNameW( nullptr, filepath.get(), l );
    if ( copied == 0 )
      return std::wstring();
    if ( copied < l )
      break;

    l += MAX_PATH;
  }
  return std::wstring( filepath.get() );
}

std::wstring dirnameOf( const std::wstring &path )
{
  const size_t pos = path.find_last_of( L"\\/" );
  if ( pos == std::wstring::npos )
    return std::wstring( L"." );
  if ( pos == 0 )
    return path.substr( 0, 1 );
  return path.substr( 0, pos );
}

std::wstring parentDir( const std::wstring &path )
{
  return dirnameOf( path );
}

void replaceAll( std::wstring &haystack, const std::wstring &from, const std::wstring &to )
{
  if ( from.empty() )
    return;
  size_t start = 0;
  while ( ( start = haystack.find( from, start ) ) != std::wstring::npos )
  {
    haystack.replace( start, from.length(), to );
    start += to.length();
  }
}

std::wstring expandEnvStrings( const std::wstring &value )
{
  DWORD needed = ExpandEnvironmentStringsW( value.c_str(), nullptr, 0 );
  if ( needed == 0 )
    return value;
  std::vector<wchar_t> buf( needed );
  if ( ExpandEnvironmentStringsW( value.c_str(), buf.data(), needed ) == 0 )
    return value;
  return std::wstring( buf.data() );
}

std::wstring utf8ToWide( const std::string &s )
{
  if ( s.empty() )
    return std::wstring();
  const int n = MultiByteToWideChar( CP_UTF8, 0, s.c_str(), -1, nullptr, 0 );
  if ( n <= 0 )
    return std::wstring();
  std::wstring out( static_cast<size_t>( n - 1 ), L'\0' );
  MultiByteToWideChar( CP_UTF8, 0, s.c_str(), -1, &out[0], n );
  return out;
}

bool putEnvVar( const std::wstring &name, const std::wstring &value )
{
  // _wputenv requires "NAME=value". Windows process env block is limited;
  // a PATH that embeds the full machine PATH often fails here.
  const std::wstring assignment = name + L"=" + value;
  return _wputenv( assignment.c_str() ) == 0;
}

bool applyEnvLine( const std::wstring &rawLine, const std::wstring &appDir, const std::wstring &prefixDir )
{
  std::wstring line = rawLine;
  // Trim CR (Windows newlines) and skip blanks / comments
  if ( !line.empty() && line.back() == L'\r' )
    line.pop_back();
  if ( line.empty() || line[0] == L'#' )
    return true;

  const size_t eq = line.find( L'=' );
  if ( eq == std::wstring::npos )
    return true;

  std::wstring name = line.substr( 0, eq );
  std::wstring value = line.substr( eq + 1 );
  replaceAll( value, L"{app}", appDir );
  replaceAll( value, L"{prefix}", prefixDir );
  value = expandEnvStrings( value );

  // Keep PATH short: prepend our bin dir, do not require shipping the whole system PATH.
  if ( _wcsicmp( name.c_str(), L"PATH" ) == 0 )
  {
    const wchar_t *oldPath = _wgetenv( L"PATH" );
    std::wstring merged = value;
    if ( oldPath && *oldPath )
    {
      // Prefer our directories first so DLL resolution finds install\bin without a huge .env PATH.
      merged = value + L";" + oldPath;
    }
    // If the merged PATH is enormous, fall back to install bin only + system32 essentials.
    if ( merged.size() > 30000 )
    {
      wchar_t windir[MAX_PATH] = {};
      GetWindowsDirectoryW( windir, MAX_PATH );
      merged = value + L";" + windir + L";" + std::wstring( windir ) + L"\\system32;" + std::wstring( windir ) + L"\\system32\\WBem";
    }
    return putEnvVar( name, merged );
  }

  return putEnvVar( name, value );
}

std::vector<std::wstring> defaultEnvLines( const std::wstring &appDir, const std::wstring &prefixDir )
{
  return {
    L"PATH=" + appDir,
    L"QGIS_PREFIX_PATH=" + prefixDir,
    L"PROJ_DATA=" + prefixDir + L"\\share\\proj",
    // Override any machine-global PROJ_LIB (PostGIS etc.) — same path as PROJ_DATA.
    L"PROJ_LIB=" + prefixDir + L"\\share\\proj",
    L"GDAL_DATA=" + prefixDir + L"\\share\\gdal",
    L"QT_PLUGIN_PATH=" + appDir + L"\\Qt6\\plugins;" + appDir + L"\\Qt6\\plugins\\crypto;" + appDir + L"\\plugins",
    L"PYTHONHOME=" + appDir,
    L"PYTHONPATH=" + prefixDir + L"\\python;" + prefixDir + L"\\python\\plugins;" + appDir + L"\\Lib;" + appDir + L"\\Lib\\site-packages;" + appDir + L"\\DLLs",
  };
}

void addDllSearchDir( const std::wstring &dir,
                      BOOL ( *SetDefaultDllDirectories )( DWORD ),
                      DLL_DIRECTORY_COOKIE ( *AddDllDirectory )( PCWSTR ) )
{
  if ( !SetDefaultDllDirectories || !AddDllDirectory )
    return;
  if ( !dir.empty() )
    AddDllDirectory( dir.c_str() );
}

bool readUtf8FileLines( const std::wstring &path, std::list<std::wstring> &lines )
{
  FILE *fp = _wfopen( path.c_str(), L"r" );
  if ( !fp )
    return false;

  char buf[4096];
  while ( fgets( buf, sizeof( buf ), fp ) )
  {
    std::string line( buf );
    if ( !line.empty() && line.back() == '\n' )
      line.pop_back();
    lines.push_back( utf8ToWide( line ) );
  }
  fclose( fp );
  return true;
}

} // namespace

int CALLBACK WinMain( HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/ )
{
  const std::wstring exename( moduleExePath() );
  const std::wstring basename( exename.substr( 0, exename.size() - 4 ) );
  const std::wstring appDir = dirnameOf( exename );
  const std::wstring prefixDir = parentDir( appDir );

  if ( _wgetenv( L"OSGEO4W_ROOT" ) && __argc == 2 && strcmp( __argv[1], "--postinstall" ) == 0 )
  {
    const std::wstring envfile( basename + L".env" );

    // write or update environment file
    if ( _waccess( envfile.c_str(), 0 ) < 0 || _waccess( envfile.c_str(), 2 ) == 0 )
    {
      std::list<std::wstring> vars;
      if ( !readUtf8FileLines( basename + L".vars", vars ) )
      {
        showError( L"Could not read environment variable list " + basename + L".vars", L"Error loading Hake GIS" );
        return EXIT_FAILURE;
      }

      FILE *file = _wfopen( envfile.c_str(), L"w" );
      if ( !file )
      {
        showError( L"Could not write environment file " + basename + L".env", L"Error loading Hake GIS" );
        return EXIT_FAILURE;
      }

      for ( const std::wstring &var : vars )
      {
        const wchar_t *value = _wgetenv( var.c_str() );
        if ( value )
          fwprintf( file, L"%ls=%ls\n", var.c_str(), value );
      }
      fclose( file );
    }

    return EXIT_SUCCESS;
  }

  // Prefer packaged .env; if missing or unreadable, synthesize a short install-local env.
  {
    std::list<std::wstring> envLines;
    const bool haveEnvFile = readUtf8FileLines( basename + L".env", envLines );
    bool appliedAny = false;
    if ( haveEnvFile )
    {
      for ( const std::wstring &var : envLines )
      {
        if ( !applyEnvLine( var, appDir, prefixDir ) )
        {
          // PATH overflow / env-block full: fall back to a minimal PATH and continue.
          if ( var.rfind( L"PATH=", 0 ) == 0 || var.rfind( L"path=", 0 ) == 0 )
          {
            putEnvVar( L"PATH", appDir );
            continue;
          }
          showError( L"Could not set environment variable (environment block may be full):\n" + var
                       + L"\n\nHelp: shorten the Windows system PATH, or edit:\n" + basename + L".env",
                     L"Error loading Hake GIS" );
          return EXIT_FAILURE;
        }
        appliedAny = true;
      }
    }
    if ( !appliedAny )
    {
      const auto defaults = defaultEnvLines( appDir, prefixDir );
      for ( const std::wstring &line : defaults )
      {
        if ( !applyEnvLine( line, appDir, prefixDir ) )
        {
          putEnvVar( L"PATH", appDir );
        }
      }
    }
  }

#ifndef _MSC_VER // MinGW
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
  HINSTANCE hKernelDLL = LoadLibraryA( "kernel32.dll" );
  BOOL ( *SetDefaultDllDirectories )( DWORD ) = hKernelDLL ? reinterpret_cast<BOOL ( * )( DWORD )>( GetProcAddress( hKernelDLL, "SetDefaultDllDirectories" ) ) : nullptr;
  DLL_DIRECTORY_COOKIE ( *AddDllDirectory )( PCWSTR ) = hKernelDLL ? reinterpret_cast<DLL_DIRECTORY_COOKIE ( * )( PCWSTR )>( GetProcAddress( hKernelDLL, "AddDllDirectory" ) ) : nullptr;
#ifndef _MSC_VER // MinGW
#pragma GCC diagnostic pop
#endif

  if ( SetDefaultDllDirectories && AddDllDirectory )
  {
    SetDefaultDllDirectories( LOAD_LIBRARY_SEARCH_DEFAULT_DIRS );

    // Always search the install bin directory first (independent of PATH length).
    addDllSearchDir( appDir, SetDefaultDllDirectories, AddDllDirectory );

    wchar_t windir[MAX_PATH];
    GetWindowsDirectoryW( windir, MAX_PATH );
    wchar_t systemdir[MAX_PATH];
    GetSystemDirectoryW( systemdir, MAX_PATH );

    wchar_t *path = _wgetenv( L"PATH" ) ? wcsdup( _wgetenv( L"PATH" ) ) : nullptr;
    if ( path )
    {
#ifdef _UCRT
      for ( wchar_t *p = wcstok( path, L";", nullptr ); p; p = wcstok( nullptr, L";", nullptr ) )
#else
      for ( wchar_t *p = wcstok( path, L";" ); p; p = wcstok( nullptr, L";" ) )
#endif
      {
        if ( _wcsicmp( p, windir ) == 0 )
          continue;
        if ( _wcsicmp( p, systemdir ) == 0 )
          continue;
        AddDllDirectory( p );
      }
      free( path );
    }
  }

#ifndef QGIS_APP_DLL_NAME
#ifdef _MSC_VER
#define QGIS_APP_DLL_NAME "qgis_app.dll"
#else
// MinGW
#define QGIS_APP_DLL_NAME "libqgis_app.dll"
#endif
#endif

  HINSTANCE hGetProcIDDLL = LoadLibraryA( QGIS_APP_DLL_NAME );

  if ( !hGetProcIDDLL )
  {
    DWORD error = GetLastError();
    LPWSTR errorText = nullptr;

    FormatMessageW( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), reinterpret_cast<LPWSTR>( &errorText ), 0, nullptr );

    showError( std::wstring( L"Could not load " ) + utf8ToWide( QGIS_APP_DLL_NAME ) + L" \n Windows Error: " + ( errorText ? errorText : L"" )
                 + L"\n Help: \n\n Check " + basename + L".env for correct environment paths"
                 + L"\n Ensure " + utf8ToWide( QGIS_APP_DLL_NAME ) + L" exists in:\n " + appDir,
               L"Error loading Hake GIS" );

    LocalFree( errorText );
    return EXIT_FAILURE;
  }

#ifndef _MSC_VER // MinGW
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
  int ( *realmain )( int, char *[] ) = ( int ( * )( int, char *[] ) ) GetProcAddress( hGetProcIDDLL, "main" );
#ifndef _MSC_VER // MinGW
#pragma GCC diagnostic pop
#endif

  if ( !realmain )
  {
    showError( std::wstring( L"Could not locate main function in " ) + utf8ToWide( QGIS_APP_DLL_NAME ), L"Error loading Hake GIS" );
    return EXIT_FAILURE;
  }

  return realmain( __argc, __argv );
}
