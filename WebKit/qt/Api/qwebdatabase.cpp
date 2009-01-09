/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "qwebdatabase.h"
#include "qwebdatabase_p.h"
#include "qwebsecurityorigin.h"
#include "qwebsecurityorigin_p.h"
#include "DatabaseDetails.h"
#include "DatabaseTracker.h"

using namespace WebCore;

/*!
    \class QWebDatabase
    \since 4.5
    \brief The QWebDatabase class provides access to HTML 5 databases created with JavaScript.

    The upcoming HTML 5 standard includes support for SQL databases that web sites can create and
    access on a local computer through JavaScript. QWebDatabase is the C++ interface to these databases.

    For more information refer to the \l{http://www.w3.org/html/wg/html5/#sql}{HTML 5 Draft Standard}.

    \sa QWebSecurityOrigin
*/

/*!
    Constructs a web database from \a other.
*/
QWebDatabase::QWebDatabase(const QWebDatabase& other) : d(other.d)
{
}

/*!
    Assigns the \a other web database to this.
*/
QWebDatabase& QWebDatabase::operator=(const QWebDatabase& other)
{
    d = other.d;
    return *this;
}

/*!
    Returns the name of the database.
*/
QString QWebDatabase::name() const
{
    return d->name;
}

/*!
    Returns the name of the database as seen by the user.
*/
QString QWebDatabase::displayName() const
{
    DatabaseDetails details = DatabaseTracker::tracker().detailsForNameAndOrigin(d->name, d->origin.get());
    return details.displayName();
}

/*!
    Returns the expected size of the database in bytes as defined by the web author.
*/
qint64 QWebDatabase::expectedSize() const
{
    DatabaseDetails details = DatabaseTracker::tracker().detailsForNameAndOrigin(d->name, d->origin.get());
    return details.expectedUsage();
}

/*!
    Returns the current size of the database in bytes.
*/
qint64 QWebDatabase::size() const
{
    DatabaseDetails details = DatabaseTracker::tracker().detailsForNameAndOrigin(d->name, d->origin.get());
    return details.currentUsage();
}

/*!
    \internal
*/
QWebDatabase::QWebDatabase(QWebDatabasePrivate* priv)
{
    d = priv;
}

/*!
    Returns the file name of the web database.

    The name can be used to access the database through the QtSql database module, for example:
    \code
      QWebDatabase webdb = ...
      QSqlDatabase sqldb = QSqlDatabase::addDatabase("QSQLITE", "myconnection");
      sqldb.setDatabaseName(webdb.fileName());
      if (sqldb.open()) {
          QStringList tables = sqldb.tables();
          ...
      }
    \endcode

    \note Concurrent access to a database from multiple threads or processes
    is not very efficient because Sqlite is used as WebKit's database backend.
*/
QString QWebDatabase::fileName() const
{
    return DatabaseTracker::tracker().fullPathForDatabase(d->origin.get(), d->name, false);
}

/*!
    Returns the databases's security origin.
*/
QWebSecurityOrigin QWebDatabase::origin() const
{
    QWebSecurityOriginPrivate* priv = new QWebSecurityOriginPrivate(d->origin.get());
    QWebSecurityOrigin origin(priv);
    return origin;
}

/*!
    Removes the database, \a db, from its security origin. All data stored in this database
    will be destroyed.
*/
void QWebDatabase::removeDatabase(const QWebDatabase &db)
{
    DatabaseTracker::tracker().deleteDatabase(db.d->origin.get(), db.d->name);
}

/*!
    Destroys the web database object. The data within this database is \b not destroyed.
*/
QWebDatabase::~QWebDatabase()
{
}
