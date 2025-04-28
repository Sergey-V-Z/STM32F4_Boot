/****************************************************************************
** Meta object code from reading C++ file 'firmware_updater.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.14.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "firmware_updater.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'firmware_updater.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.14.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_FirmwareUpdater_t {
    QByteArrayData data[23];
    char stringdata0[317];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_FirmwareUpdater_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_FirmwareUpdater_t qt_meta_stringdata_FirmwareUpdater = {
    {
QT_MOC_LITERAL(0, 0, 15), // "FirmwareUpdater"
QT_MOC_LITERAL(1, 16, 23), // "connectionStatusChanged"
QT_MOC_LITERAL(2, 40, 0), // ""
QT_MOC_LITERAL(3, 41, 9), // "connected"
QT_MOC_LITERAL(4, 51, 14), // "updateProgress"
QT_MOC_LITERAL(5, 66, 7), // "percent"
QT_MOC_LITERAL(6, 74, 11), // "blockNumber"
QT_MOC_LITERAL(7, 86, 11), // "totalBlocks"
QT_MOC_LITERAL(8, 98, 19), // "updateStatusChanged"
QT_MOC_LITERAL(9, 118, 6), // "status"
QT_MOC_LITERAL(10, 125, 5), // "error"
QT_MOC_LITERAL(11, 131, 15), // "updateCompleted"
QT_MOC_LITERAL(12, 147, 7), // "success"
QT_MOC_LITERAL(13, 155, 20), // "deviceStatusReceived"
QT_MOC_LITERAL(14, 176, 10), // "logMessage"
QT_MOC_LITERAL(15, 187, 7), // "message"
QT_MOC_LITERAL(16, 195, 17), // "onSocketConnected"
QT_MOC_LITERAL(17, 213, 20), // "onSocketDisconnected"
QT_MOC_LITERAL(18, 234, 13), // "onSocketError"
QT_MOC_LITERAL(19, 248, 28), // "QAbstractSocket::SocketError"
QT_MOC_LITERAL(20, 277, 11), // "socketError"
QT_MOC_LITERAL(21, 289, 17), // "onSocketReadyRead"
QT_MOC_LITERAL(22, 307, 9) // "onTimeout"

    },
    "FirmwareUpdater\0connectionStatusChanged\0"
    "\0connected\0updateProgress\0percent\0"
    "blockNumber\0totalBlocks\0updateStatusChanged\0"
    "status\0error\0updateCompleted\0success\0"
    "deviceStatusReceived\0logMessage\0message\0"
    "onSocketConnected\0onSocketDisconnected\0"
    "onSocketError\0QAbstractSocket::SocketError\0"
    "socketError\0onSocketReadyRead\0onTimeout"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_FirmwareUpdater[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      11,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       6,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   69,    2, 0x06 /* Public */,
       4,    3,   72,    2, 0x06 /* Public */,
       8,    2,   79,    2, 0x06 /* Public */,
      11,    1,   84,    2, 0x06 /* Public */,
      13,    2,   87,    2, 0x06 /* Public */,
      14,    1,   92,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      16,    0,   95,    2, 0x08 /* Private */,
      17,    0,   96,    2, 0x08 /* Private */,
      18,    1,   97,    2, 0x08 /* Private */,
      21,    0,  100,    2, 0x08 /* Private */,
      22,    0,  101,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::Bool,    3,
    QMetaType::Void, QMetaType::Int, QMetaType::UInt, QMetaType::UInt,    5,    6,    7,
    QMetaType::Void, QMetaType::UInt, QMetaType::UInt,    9,   10,
    QMetaType::Void, QMetaType::Bool,   12,
    QMetaType::Void, QMetaType::UInt, QMetaType::UInt,    9,   10,
    QMetaType::Void, QMetaType::QString,   15,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 19,   20,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void FirmwareUpdater::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<FirmwareUpdater *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->connectionStatusChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 1: _t->updateProgress((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< quint32(*)>(_a[2])),(*reinterpret_cast< quint32(*)>(_a[3]))); break;
        case 2: _t->updateStatusChanged((*reinterpret_cast< quint32(*)>(_a[1])),(*reinterpret_cast< quint32(*)>(_a[2]))); break;
        case 3: _t->updateCompleted((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 4: _t->deviceStatusReceived((*reinterpret_cast< quint32(*)>(_a[1])),(*reinterpret_cast< quint32(*)>(_a[2]))); break;
        case 5: _t->logMessage((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 6: _t->onSocketConnected(); break;
        case 7: _t->onSocketDisconnected(); break;
        case 8: _t->onSocketError((*reinterpret_cast< QAbstractSocket::SocketError(*)>(_a[1]))); break;
        case 9: _t->onSocketReadyRead(); break;
        case 10: _t->onTimeout(); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 8:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QAbstractSocket::SocketError >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (FirmwareUpdater::*)(bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&FirmwareUpdater::connectionStatusChanged)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (FirmwareUpdater::*)(int , quint32 , quint32 );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&FirmwareUpdater::updateProgress)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (FirmwareUpdater::*)(quint32 , quint32 );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&FirmwareUpdater::updateStatusChanged)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (FirmwareUpdater::*)(bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&FirmwareUpdater::updateCompleted)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (FirmwareUpdater::*)(quint32 , quint32 );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&FirmwareUpdater::deviceStatusReceived)) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (FirmwareUpdater::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&FirmwareUpdater::logMessage)) {
                *result = 5;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject FirmwareUpdater::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_FirmwareUpdater.data,
    qt_meta_data_FirmwareUpdater,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *FirmwareUpdater::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *FirmwareUpdater::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_FirmwareUpdater.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int FirmwareUpdater::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    return _id;
}

// SIGNAL 0
void FirmwareUpdater::connectionStatusChanged(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void FirmwareUpdater::updateProgress(int _t1, quint32 _t2, quint32 _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void FirmwareUpdater::updateStatusChanged(quint32 _t1, quint32 _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void FirmwareUpdater::updateCompleted(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void FirmwareUpdater::deviceStatusReceived(quint32 _t1, quint32 _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void FirmwareUpdater::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
