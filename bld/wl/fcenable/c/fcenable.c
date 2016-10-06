/****************************************************************************
*
*                            Open Watcom Project
*
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  Far call optimization enabling utility.
*
****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <malloc.h>
#include <setjmp.h>
#if defined( __WATCOMC__ ) || !defined( __UNIX__ )
#include <process.h>
#endif
#ifdef __UNIX__
#include <utime.h>
#else
#include <sys/utime.h>
#endif
#include "wio.h"
#include "fcenable.h"
#include "banner.h"

#include "clibext.h"


#define NOFILE -1
#define STDOUT_HANDLE 1

#ifdef _WIN64
#define posix_read      __w64_read
#define posix_write     __w64_write
#else
#define posix_read      read
#define posix_write     write
#endif

typedef enum {
    EX_NONE,
    EX_GOT_SEG,
    EX_GOT_START,
    EX_GOT_DASH,
    EX_GOT_COLON
} EX_STATE;

int                 InFile;
int                 OutFile;
name_list           *ClassList = NULL;
name_list           *SegList = NULL;
exclude_list        *ExcludeList = NULL;

static bool         MakeBackup = true;
static bool         NewOption;
static EX_STATE     ExcludeState;
static exclude_list *ExEntry;
static void         *InputBuffer = NULL;

static char         *HelpMsg =
"Usage: FCENABLE { [options] [files] }\n"
"This allows WLINK to do far call optimization on non-WATCOM object files\n"
"Options are:\n"
"-b                don't produce a backup file\n"
"-c <class_list>   allow optimization for specified classes (default CODE)\n"
"-s <seg_list>     allow optimization for specified segments\n"
"-x <exclude_list> exclude specified area when optimizing\n"
"class_list    ::= class_name {,class_name}\n"
"seg_list      ::= seg_name {,seg_name}\n"
"exclude_list  ::= exclude {,exclude}\n"
"exclude       ::= seg_name start_offset (-end_offset | :length)\n"
"files can be object files, concatenated object files, or library files\n"
"file names may contain wild cards\n"
;

#define TEMP_OBJ_NAME   "_@FCOBJ_.%$%"
#define TEMP_LIB_NAME   "_@FCLIB_.%$%"

#define DEF_CLASS       "CODE"

// forward declarations
static void     ProcessFiles( char ** );
static void     ProcFile( char *fname );


// the spawn & suicide support.

static void *SpawnStack;

static int Spawn1( void (*fn)(char **), char **data1 )
/****************************************************/
{
    void    *save_env;
    jmp_buf env;
    int     status;

    save_env = SpawnStack;
    SpawnStack = env;
    status = setjmp( env );
    if( status == 0 ) {
        (*fn)( data1 );
    }
    SpawnStack = save_env;  /* unwind */
    return( status );
}


static void Suicide( void )
/*************************/
{
    if( SpawnStack != NULL ) {
        longjmp( SpawnStack, 1 );
    }
}


static int QOpen( const char *filename, int access, int permission )
/******************************************************************/
{
    int result;

    result = open( filename, access, permission );
    if( result == -1 ) {
        IOError( "problem opening file" );
    }
    return( result );
}


static void QRemove( const char *filename )
/*****************************************/
{
    if( remove( filename ) != 0 ) {
        IOError( "problem removing file" );
    }
}


int main(int argc, char **argv )
/******************************/
{
    int     retval = 0;

    MemInit();
#if defined( _BETAVER )
    put( banner1w1( "Far Call Optimization Enabling Utility" ) );
    put( banner1w2( _FCENABLE_VERSION_ ) );
#else
    put( banner1w( "Far Call Optimization Enabling Utility", _FCENABLE_VERSION_ ) );
#endif
    put( banner2 );
    put( banner2a( "1990" ) );
    put( banner3 );
    put( banner3a );
    InputBuffer = InitRecStuff();
    InFile = NOFILE;
    OutFile = NOFILE;
    ClassList = MemAlloc( sizeof( name_list ) + sizeof( DEF_CLASS ) - 1 );
    ClassList->next = NULL;
    ClassList->lnameidx = 0;
    memcpy( ClassList->name, DEF_CLASS, sizeof( DEF_CLASS ) - 1 );
    if( ( argc < 2 ) || ( argv[1][0] == '?' ) ) {
        put( HelpMsg );
    } else {
        argv++;     // skip the program name
        retval = Spawn1( ProcessFiles, argv );
    }
    FinalCleanup();
    MemFini();
    return( retval );
}

static void ProcList( bool (*fn)(const char *,size_t), char ***argv )
/*******************************************************************/
// this processes a list of comma-separated strings, being as forgiving about
// spaces as possible.
{
    char    *item;
    char    *comma;
    bool    checksep;   // true iff we should check for a separator.

    (**argv)++;        // skip the option character.
    checksep = false;
    for( ; **argv != NULL; (*argv)++ ) {
        item = **argv;
        if( checksep ) {                // separator needed to continue list
            if( *item != ',' )
                break;
            item++;
        }
        // while commas inside string
        for( comma = strchr( item, ',' ); comma != NULL; comma = strchr( item, ',' ) ) {
            if( !fn( item, comma - item ) ) {
                Warning( "ignoring unexpected comma" );
            }
            item = comma + 1;
        }
        if( *item != '\0' ) {
            checksep = fn( item, strlen( item ) );
        } else {        // we had a comma at end of string, so no sep needed
            checksep = false;
        }
    }
    if( ExcludeState != EX_NONE ) {
        Warning( "incomplete exclude option specified" );
        MemFree( ExEntry );
        ExcludeState = EX_NONE;
    }
}

static void MakeListItem( name_list **list, const char *item, size_t len )
/************************************************************************/
{
    name_list *entry;

    entry = MemAlloc( sizeof( name_list ) + len );
    entry->next = NULL;
    entry->lnameidx = 0;
    memcpy( entry->name, item, len );
    *(entry->name + len) = '\0';
    LinkList( (void **)list, entry );
}

static bool ProcClass( const char *item, size_t len )
/***************************************************/
{
    if( NewOption ) {
        FreeList( ClassList );
        NewOption = false;
        ClassList = NULL;
    }
    MakeListItem( &ClassList, item, len );
    return( true );     // true == check for a list separator
}

static bool ProcSeg( const char *item, size_t len )
/*************************************************/
{
    if( NewOption ) {
        FreeList( SegList );
        NewOption = false;
        SegList = NULL;
    }
    MakeListItem( &SegList, item, len );
    return( true );     // true == check for a list separator
}

static bool ProcExclude( const char *item, size_t len )
/*****************************************************/
{
    char *endptr;

    while( len > 0 ) {
        switch( ExcludeState ) {
        case EX_NONE:
            ExEntry = MemAlloc( sizeof( exclude_list ) + len );
            ExEntry->next = NULL;
            ExEntry->segidx = 0;
            ExEntry->lnameidx = 0;
            memcpy( ExEntry->name, item, len );
            *(ExEntry->name + len ) = '\0';
            ExcludeState = EX_GOT_SEG;
            return( false );
        case EX_GOT_SEG:
            ExEntry->start_off = strtoul( item, &endptr, 16 );
            len -= endptr - item;
            item = endptr;
            ExcludeState = EX_GOT_START;
            break;
        case EX_GOT_START:
            if( *item == '-' ) {
                ExcludeState = EX_GOT_DASH;
            } else if( *item == ':' ) {
                ExcludeState = EX_GOT_COLON;
            } else {
                Warning( "invalid range separator" );
                ExcludeState = EX_NONE;
                MemFree( ExEntry );
                return( true );
            }
            item++;
            len--;
            break;
        default:    // EX_GOT_DASH or EX_GOT_COLON
            ExEntry->end_off = strtoul( item, NULL, 16 );
            if( ExcludeState == EX_GOT_COLON ) {
                ExEntry->end_off += ExEntry->start_off;
            }
            LinkList( (void **)&ExcludeList, ExEntry );
            ExcludeState = EX_NONE;
            return( true );     // want a list separator after this.
        }
    }
    return( false );
}

static void ProcessOption( char ***argv )
/***************************************/
{
    char    *item;
    char    option;

    NewOption = true;
    (**argv)++;        // skip the switch character.
    item = **argv;
    option = tolower( *item );
    switch( option ) {
    case 'c':           // class list.
        ProcList( ProcClass, argv );
        break;
    case 's':
        ProcList( ProcSeg, argv );
        break;
    case 'x':
        ProcList( ProcExclude, argv );
        break;
    case 'b':
        MakeBackup = false;
        (*argv)++;
        break;
    default:
        put( "option not recognized: " );
        putlen( item, 1 );
        put( "\n" );
        (*argv)++;
    }
}

static void ProcessFiles( char **argv )
/*************************************/
{
    char    *item;

    while( *argv != NULL ) {
        item = *argv;
        if( *item == '-' || *item == '/' ) {
            ProcessOption( &argv );
        } else {
            put( "Processing file '" );
            put( item );
            put( "'\n" );
            ProcFile( item );
            argv++;
        }
    }
}

static void CloseFiles( void )
/****************************/
{
    if( InFile != NOFILE ) {
        close( InFile );
        InFile = NOFILE;
    }
    if( OutFile != NOFILE ) {
        close( OutFile );
        OutFile = NOFILE;
    }
}

static void ReplaceExt( char *name, char *new_ext, bool force )
/*************************************************************/
{
    char        buff[_MAX_PATH2];
    char        *p;
    char        *d;
    char        *n;
    char        *e;

    _splitpath2( name, buff, &d, &p, &n, &e );
    if( force || e[0] == '\0' ) {
        strcpy( e, new_ext );
        _makepath( name, d, p, n, e );
    }
}


#if defined( __UNIX__ )
#define WLIB_EXE "wlib"
#else
#define WLIB_EXE "wlib.exe"
#endif

static void DoReplace( void )
/***************************/
// this supports concatenated object decks and libraries (PageLen != 0)
{
    FlushBuffer();
    if( PageLen != 0 ) {        // NYI - spawning WLIB every time is
        close( OutFile );       // rather slow. Replace this somehow??
        OutFile = NOFILE;
        if( spawnlp( P_WAIT, WLIB_EXE, WLIB_EXE, TEMP_LIB_NAME, "-b", "+" TEMP_OBJ_NAME, NULL ) != 0 ) {
            Error( "problem with temporary library" );
        }
        QRemove( TEMP_OBJ_NAME );
    }
}

static void ProcFile( char *fname )
/*********************************/
{
    int         ftype;
    char        *name;
    int         status;
    int         namelen;
    char        *bak;

    namelen = strlen( fname ) + 5;
    name = alloca( namelen );
    if( name == NULL )      // null == no stack space left.
        Suicide();
    strcpy( name, fname );
    ReplaceExt( name, ".obj", false );
    InFile = QOpen( name, O_RDONLY | O_BINARY, 0 );
    for( ;; ) {
        CleanRecStuff();
        ftype = ReadRec();
        if( ftype == ENDLIBRARY || ftype == ENDFILE ) {
            break;
        } else if( ftype == LIBRARY ) {
            Warning( "exclude option does not apply to libraries" );
            FreeList( ExcludeList );
            ExcludeList = NULL;
        } else if( ftype != OBJECT ) {
            Error( "file is not a standard OBJECT or LIBRARY file" );
        }
        OutFile = QOpen( TEMP_OBJ_NAME, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0 );
        do {
            ProcessRec();
            status = ReadRec();
        } while( status == OK );
        if( status == ENDMODULE ) {
            ProcessRec();           // process the modend rec.
            DoReplace();
        } else {
            Error( "premature end of file encountered" );
        }
        FreeList( ExcludeList );    // do this here so concatenated .obj files
        ExcludeList = NULL;         // only have the first module excluded.
    }
    CloseFiles();
    if( MakeBackup ) {
        bak = alloca( namelen );
        if( bak == NULL )           // null == no stack space left.
            Suicide();
        strcpy( bak, name );
        if( ftype == ENDLIBRARY ) {
            ReplaceExt( bak, ".bak", true );
        } else {
            ReplaceExt( bak, ".bob", true );
        }
        CopyFile( name, bak );
    }
    QRemove( name );
    if( ftype == ENDLIBRARY ) {
        rename( TEMP_LIB_NAME, name );
    } else {
        rename( TEMP_OBJ_NAME, name );
    }
    FileCleanup();
}

int CopyFile( const char * file1, const char * file2 )
/****************************************************/
{
    size_t                  len;
    auto struct stat        statblk;
    auto struct utimbuf     utimebuf;

    remove( file2 );
    OutFile = NOFILE;
    InFile = QOpen( file1, O_RDONLY | O_BINARY, 0 );
    OutFile = QOpen( file2, O_WRONLY | O_TRUNC | O_CREAT | O_BINARY, 0 );
    while( (len = QRead( InFile, InputBuffer, MAX_OBJECT_REC_SIZE )) != 0 ) {
        QWrite( OutFile, InputBuffer, len );
    }
    CloseFiles();
    if( stat( file1, &statblk ) == 0 ) {
        utimebuf.actime = statblk.st_atime;
        utimebuf.modtime = statblk.st_mtime;
        utime( file2, &utimebuf );
    }
    return( OK );
}

void put( const char * str )
/**************************/
{
    posix_write( STDOUT_HANDLE, str, strlen( str ) );
}

void putlen( const char *str, size_t len )
/****************************************/
{
    posix_write( STDOUT_HANDLE, str, len );
}

// these are utility routines used frequently in TDCVT.

typedef struct _node {      // structure used for list traversal routines.
    struct _node *  next;
} node;

void LinkList( void **in_head, void *newnode )
/********************************************/
/* Link a new node into a linked list (new node goes at the end of the list) */
{
    node                **owner;

    for( owner = (node **)in_head; *owner != NULL; ) {
        owner = &(*owner)->next;
    }
    *owner = newnode;
}

void FreeList( void *list )
/*************************/
/* Free a list of nodes. */
{
    node *  next;
    node *  curr;

    for( curr = (node *)list; curr != NULL; curr = next ) {
        next = curr->next;
        MemFree( curr );
    }
}

void Warning( const char *msg )
/*****************************/
{
    put( "warning: " );
    put( msg );
    put( "\n" );
}

void Error( const char *msg )
/***************************/
{
    put( "Error: " );
    put( msg );
    put( "\n" );
    Suicide();
}

void IOError( const char *msg )
/*****************************/
{
    put( msg );
    put( ": " );
    put( strerror( errno ) );
    put( "\n" );
    Suicide();
}

// these are the file i/o routines for tdcvt.

size_t QRead( int handle, void *buffer, size_t len )
/**************************************************/
{
    size_t result;

    result = posix_read( handle, buffer, len );
    if( result == -1 ) {
        IOError( "problem reading file" );
    }
    return( result );
}

size_t QWrite( int handle, const void *buffer, size_t len )
/*********************************************************/
{
    size_t result;

    result = posix_write( handle, buffer, len );
    if( result == -1 ) {
        IOError( "problem writing file" );
    } else if( result != len ) {
        Error( "disk full" );
    }
    return( result );
}

long int QSeek( int handle, long offset, int origin )
/***************************************************/
{
    long int result;

    result = lseek( handle, offset, origin );
    if( result == -1L ) {
        IOError( "problem during seek" );
    }
    return( result );
}
