/****************************************************************************
** Meta object code from reading C++ file 'ApiClient.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.12.8)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "ApiClient.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ApiClient.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.12.8. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_xumj__ApiClient_t {
    QByteArrayData data[12];
    char stringdata0[133];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_xumj__ApiClient_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_xumj__ApiClient_t qt_meta_stringdata_xumj__ApiClient = {
    {
QT_MOC_LITERAL(0, 0, 15), // "xumj::ApiClient"
QT_MOC_LITERAL(1, 16, 12), // "logsReceived"
QT_MOC_LITERAL(2, 29, 0), // ""
QT_MOC_LITERAL(3, 30, 4), // "logs"
QT_MOC_LITERAL(4, 35, 10), // "totalCount"
QT_MOC_LITERAL(5, 46, 16), // "logStatsReceived"
QT_MOC_LITERAL(6, 63, 5), // "stats"
QT_MOC_LITERAL(7, 69, 20), // "healthStatusReceived"
QT_MOC_LITERAL(8, 90, 7), // "healthy"
QT_MOC_LITERAL(9, 98, 7), // "message"
QT_MOC_LITERAL(10, 106, 13), // "errorOccurred"
QT_MOC_LITERAL(11, 120, 12) // "errorMessage"

    },
    "xumj::ApiClient\0logsReceived\0\0logs\0"
    "totalCount\0logStatsReceived\0stats\0"
    "healthStatusReceived\0healthy\0message\0"
    "errorOccurred\0errorMessage"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_xumj__ApiClient[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    2,   34,    2, 0x06 /* Public */,
       5,    1,   39,    2, 0x06 /* Public */,
       7,    2,   42,    2, 0x06 /* Public */,
      10,    1,   47,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QJsonArray, QMetaType::Int,    3,    4,
    QMetaType::Void, QMetaType::QJsonObject,    6,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,    8,    9,
    QMetaType::Void, QMetaType::QString,   11,

       0        // eod
};

void xumj::ApiClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ApiClient *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->logsReceived((*reinterpret_cast< const QJsonArray(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 1: _t->logStatsReceived((*reinterpret_cast< const QJsonObject(*)>(_a[1]))); break;
        case 2: _t->healthStatusReceived((*reinterpret_cast< bool(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 3: _t->errorOccurred((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ApiClient::*)(const QJsonArray & , int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ApiClient::logsReceived)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (ApiClient::*)(const QJsonObject & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ApiClient::logStatsReceived)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (ApiClient::*)(bool , const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ApiClient::healthStatusReceived)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (ApiClient::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ApiClient::errorOccurred)) {
                *result = 3;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject xumj::ApiClient::staticMetaObject = { {
    &QObject::staticMetaObject,
    qt_meta_stringdata_xumj__ApiClient.data,
    qt_meta_data_xumj__ApiClient,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *xumj::ApiClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *xumj::ApiClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_xumj__ApiClient.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int xumj::ApiClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void xumj::ApiClient::logsReceived(const QJsonArray & _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void xumj::ApiClient::logStatsReceived(const QJsonObject & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void xumj::ApiClient::healthStatusReceived(bool _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void xumj::ApiClient::errorOccurred(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
