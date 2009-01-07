//-----------------------------------------------------------------------------
// Subscription.c
//   Defines the routines for handling Oracle subscription information.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// structures used for handling subscriptions
//-----------------------------------------------------------------------------
typedef struct {
    PyObject_HEAD
    OCISubscription *handle;
    udt_Connection *connection;
    PyObject *callback;
    ub4 namespace;
    ub4 protocol;
    ub4 timeout;
    ub4 operations;
    ub4 rowids;
} udt_Subscription;

typedef struct {
    PyObject_HEAD
    ub4 type;
    PyObject *dbname;
    PyObject *tables;
} udt_Message;


//-----------------------------------------------------------------------------
// Declaration of subscription functions
//-----------------------------------------------------------------------------
static void Subscription_Free(udt_Subscription*);
static PyObject *Subscription_Repr(udt_Subscription*);
static PyObject *Subscription_RegisterQuery(udt_Subscription*, PyObject*);
static void Message_Free(udt_Message*);

//-----------------------------------------------------------------------------
// declaration of members for Python types
//-----------------------------------------------------------------------------
static PyMemberDef g_SubscriptionTypeMembers[] = {
    { "callback", T_OBJECT, offsetof(udt_Subscription, callback), READONLY },
    { "connection", T_OBJECT, offsetof(udt_Subscription, connection),
            READONLY },
    { "namespace", T_INT, offsetof(udt_Subscription, namespace), READONLY },
    { "protocol", T_INT, offsetof(udt_Subscription, protocol), READONLY },
    { "timeout", T_INT, offsetof(udt_Subscription, timeout), READONLY },
    { "operations", T_INT, offsetof(udt_Subscription, operations), READONLY },
    { "rowids", T_BOOL, offsetof(udt_Subscription, rowids), READONLY },
    { NULL }
};


static PyMemberDef g_MessageTypeMembers[] = {
    { "type", T_INT, offsetof(udt_Message, type), READONLY },
    { "dbname", T_OBJECT, offsetof(udt_Message, dbname), READONLY },
    { "tables", T_OBJECT, offsetof(udt_Message, tables), READONLY },
    { NULL }
};


//-----------------------------------------------------------------------------
// declaration of methods for Python types
//-----------------------------------------------------------------------------
static PyMethodDef g_SubscriptionTypeMethods[] = {
    { "registerquery", (PyCFunction) Subscription_RegisterQuery,
            METH_VARARGS },
    { NULL, NULL }
};


//-----------------------------------------------------------------------------
// Python type declarations
//-----------------------------------------------------------------------------
static PyTypeObject g_SubscriptionType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "cx_Oracle.Subscription",           // tp_name
    sizeof(udt_Subscription),           // tp_basicsize
    0,                                  // tp_itemsize
    (destructor) Subscription_Free,     // tp_dealloc
    0,                                  // tp_print
    0,                                  // tp_getattr
    0,                                  // tp_setattr
    0,                                  // tp_compare
    (reprfunc) Subscription_Repr,       // tp_repr
    0,                                  // tp_as_number
    0,                                  // tp_as_sequence
    0,                                  // tp_as_mapping
    0,                                  // tp_hash
    0,                                  // tp_call
    0,                                  // tp_str
    0,                                  // tp_getattro
    0,                                  // tp_setattro
    0,                                  // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                 // tp_flags
    0,                                  // tp_doc
    0,                                  // tp_traverse
    0,                                  // tp_clear
    0,                                  // tp_richcompare
    0,                                  // tp_weaklistoffset
    0,                                  // tp_iter
    0,                                  // tp_iternext
    g_SubscriptionTypeMethods,          // tp_methods
    g_SubscriptionTypeMembers,          // tp_members
    0,                                  // tp_getset
    0,                                  // tp_base
    0,                                  // tp_dict
    0,                                  // tp_descr_get
    0,                                  // tp_descr_set
    0,                                  // tp_dictoffset
    0,                                  // tp_init
    0,                                  // tp_alloc
    0,                                  // tp_new
    0,                                  // tp_free
    0,                                  // tp_is_gc
    0                                   // tp_bases
};


static PyTypeObject g_MessageType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "cx_Oracle.Message",                // tp_name
    sizeof(udt_Message),                // tp_basicsize
    0,                                  // tp_itemsize
    (destructor) Message_Free,          // tp_dealloc
    0,                                  // tp_print
    0,                                  // tp_getattr
    0,                                  // tp_setattr
    0,                                  // tp_compare
    0,                                  // tp_repr
    0,                                  // tp_as_number
    0,                                  // tp_as_sequence
    0,                                  // tp_as_mapping
    0,                                  // tp_hash
    0,                                  // tp_call
    0,                                  // tp_str
    0,                                  // tp_getattro
    0,                                  // tp_setattro
    0,                                  // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                 // tp_flags
    0,                                  // tp_doc
    0,                                  // tp_traverse
    0,                                  // tp_clear
    0,                                  // tp_richcompare
    0,                                  // tp_weaklistoffset
    0,                                  // tp_iter
    0,                                  // tp_iternext
    0,                                  // tp_methods
    g_MessageTypeMembers,               // tp_members
    0,                                  // tp_getset
    0,                                  // tp_base
    0,                                  // tp_dict
    0,                                  // tp_descr_get
    0,                                  // tp_descr_set
    0,                                  // tp_dictoffset
    0,                                  // tp_init
    0,                                  // tp_alloc
    0,                                  // tp_new
    0,                                  // tp_free
    0,                                  // tp_is_gc
    0                                   // tp_bases
};


//-----------------------------------------------------------------------------
// Message_Initialize()
//   Initialize a new message with the information from the descriptor.
//-----------------------------------------------------------------------------
static int Message_Initialize(
    udt_Message *self,                  // message object to initialize
    udt_Environment *env,               // environment to use
    dvoid *descriptor)                  // descriptor to get information from
{
    ub4 dbnameLength;
    char *dbname;
    sword status;

    // determine type
    status = OCIAttrGet(descriptor, OCI_DTYPE_CHDES, &self->type, NULL,
            OCI_ATTR_CHDES_NFYTYPE, env->errorHandle);
    if (Environment_CheckForError(env, status,
                "Subscription_CallbackHandler(): get type") < 0)
        return -1;

    // determine database name
    status = OCIAttrGet(descriptor, OCI_DTYPE_CHDES, &dbname, &dbnameLength,
            OCI_ATTR_CHDES_DBNAME, env->errorHandle);
    if (Environment_CheckForError(env, status,
                "Subscription_CallbackHandler(): get database name") < 0)
        return -1;
    self->dbname = cxString_FromEncodedString(dbname, dbnameLength);
    if (!self->dbname)
        return -1;

    return 0;
}


//-----------------------------------------------------------------------------
// Subscription_CallbackHandler()
//   Routine that performs the actual call.
//-----------------------------------------------------------------------------
static int Subscription_CallbackHandler(
    udt_Subscription *self,             // subscription object
    udt_Environment *env,               // environment to use
    dvoid *descriptor)                  // descriptor to get information from
{
    PyObject *result, *args;
    udt_Message *message;

    // create the message
    message = (udt_Message*) g_MessageType.tp_alloc(&g_MessageType, 0);
    if (!message)
        return -1;
    if (Message_Initialize(message, env, descriptor) < 0) {
        Py_DECREF(message);
        return -1;
    }

    // create the arguments for the call
    args = PyTuple_Pack(1, message);
    Py_DECREF(message);
    if (!args)
        return -1;

    // make the actual call
    result = PyObject_Call(self->callback, args, NULL);
    Py_DECREF(args);
    if (!result)
        return -1;
    Py_DECREF(result);

    return 0;
}


//-----------------------------------------------------------------------------
// Subscription_Callback()
//   Routine that is called when a callback needs to be invoked.
//-----------------------------------------------------------------------------
static void Subscription_Callback(
    udt_Subscription *self,             // subscription object
    OCISubscription *handle,            // subscription handle
    dvoid *payload,                     // payload
    ub4 *payloadLength,                 // payload length
    dvoid *descriptor,                  // descriptor
    ub4 mode)                           // mode used
{
#ifdef WITH_THREAD
    PyThreadState *threadState;
#endif
    udt_Environment *env;

    // create new thread state, if necessary
#ifdef WITH_THREAD
    threadState = PyThreadState_Swap(NULL);
    if (threadState) {
        PyThreadState_Swap(threadState);
        threadState = NULL;
    } else {
        threadState = PyThreadState_New(g_InterpreterState);
        if (!threadState) {
            PyErr_Print();
            return;
        }
        PyEval_AcquireThread(threadState);
    }
#endif

    // perform the call
    env = Environment_NewFromScratch(0, 0);
    if (!env)
        PyErr_Print();
    else {
        if (Subscription_CallbackHandler(self, env, descriptor) < 0)
            PyErr_Print();
        Py_DECREF(env);
    }

    // restore thread state, if necessary
#ifdef WITH_THREAD
    if (threadState) {
        PyThreadState_Clear(threadState);
        PyEval_ReleaseThread(threadState);
        PyThreadState_Delete(threadState);
    }
#endif
}


//-----------------------------------------------------------------------------
// Subscription_Register()
//   Register the subscription.
//-----------------------------------------------------------------------------
static int Subscription_Register(
    udt_Subscription *self)             // subscription to register
{
    udt_Environment *env;
    sword status;

    // create the subscription handle
    env = self->connection->environment;
    status = OCIHandleAlloc(env->handle, (dvoid**) &self->handle,
            OCI_HTYPE_SUBSCRIPTION, 0, 0);
    if (Environment_CheckForError(env, status,
                "Subscription_Register(): allocate handle") < 0)
        return -1;

    // set the namespace
    status = OCIAttrSet(self->handle, OCI_HTYPE_SUBSCRIPTION,
            (dvoid*) &self->namespace, sizeof(ub4), OCI_ATTR_SUBSCR_NAMESPACE,
            env->errorHandle);
    if (Environment_CheckForError(env, status,
                "Subscription_Register(): set namespace") < 0)
        return -1;

    // set the protocol
    status = OCIAttrSet(self->handle, OCI_HTYPE_SUBSCRIPTION,
            (dvoid*) &self->protocol, sizeof(ub4), OCI_ATTR_SUBSCR_RECPTPROTO,
            env->errorHandle);
    if (Environment_CheckForError(env, status,
                "Subscription_Register(): set protocol") < 0)
        return -1;

    // set the timeout
    status = OCIAttrSet(self->handle, OCI_HTYPE_SUBSCRIPTION,
            (dvoid*) &self->timeout, sizeof(ub4), OCI_ATTR_SUBSCR_TIMEOUT,
            env->errorHandle);
    if (Environment_CheckForError(env, status,
                "Subscription_Register(): set timeout") < 0)
        return -1;

    // set the context for the callback
    status = OCIAttrSet(self->handle, OCI_HTYPE_SUBSCRIPTION,
            (dvoid*) self, 0, OCI_ATTR_SUBSCR_CTX, env->errorHandle);
    if (Environment_CheckForError(env, status,
                "Subscription_Register(): set context") < 0)
        return -1;

    // set the callback, if applicable
    if (self->callback) {
        status = OCIAttrSet(self->handle, OCI_HTYPE_SUBSCRIPTION,
                (dvoid*) Subscription_Callback, 0, OCI_ATTR_SUBSCR_CALLBACK,
                env->errorHandle);
        if (Environment_CheckForError(env, status,
                    "Subscription_Register(): set callback") < 0)
            return -1;
    }

    // set whether or not rowids are desired
    status = OCIAttrSet(self->handle, OCI_HTYPE_SUBSCRIPTION,
            (dvoid*) &self->rowids, sizeof(ub4), OCI_ATTR_CHNF_ROWIDS,
            env->errorHandle);
    if (Environment_CheckForError(env, status,
                "Subscription_Register(): set rowids") < 0)
        return -1;

    // set which operations are desired
    status = OCIAttrSet(self->handle, OCI_HTYPE_SUBSCRIPTION,
            (dvoid*) &self->operations, sizeof(ub4), OCI_ATTR_CHNF_OPERATIONS,
            env->errorHandle);
    if (Environment_CheckForError(env, status,
                "Subscription_Register(): set operations") < 0)
        return -1;

    // register the subscription
    Py_BEGIN_ALLOW_THREADS
    status = OCISubscriptionRegister(self->connection->handle,
            &self->handle, 1, env->errorHandle, OCI_DEFAULT);
    Py_END_ALLOW_THREADS
    if (Environment_CheckForError(env, status,
                "Subscription_Register(): register") < 0)
        return -1;

    return 0;
}


//-----------------------------------------------------------------------------
// Subscription_New()
//   Allocate a new subscription object.
//-----------------------------------------------------------------------------
static udt_Subscription *Subscription_New(
    udt_Connection *connection,         // connection object
    ub4 namespace,                      // namespace to use
    ub4 protocol,                       // protocol to use
    PyObject *callback,                 // callback routine
    ub4 timeout,                        // timeout (in seconds)
    ub4 operations,                     // operations to notify
    int rowids)                         // retrieve rowids?
{
    udt_Subscription *self;

    self = (udt_Subscription*)
            g_SubscriptionType.tp_alloc(&g_SubscriptionType, 0);
    if (!self)
        return NULL;
    Py_INCREF(connection);
    self->connection = connection;
    Py_XINCREF(callback);
    self->callback = callback;
    self->namespace = namespace;
    self->protocol = protocol;
    self->timeout = timeout;
    self->rowids = rowids;
    self->operations = operations;
    self->handle = NULL;
    if (Subscription_Register(self) < 0) {
        Py_DECREF(self);
        return NULL;
    }

    return self;
}


//-----------------------------------------------------------------------------
// Subscription_Free()
//   Free the memory associated with a subscription.
//-----------------------------------------------------------------------------
static void Subscription_Free(
    udt_Subscription *self)               // subscription to free
{
    if (self->handle)
        OCISubscriptionUnRegister(self->connection->handle,
                self->handle, self->connection->environment->errorHandle,
                OCI_DEFAULT);
    Py_CLEAR(self->connection);
    Py_CLEAR(self->callback);
    Py_TYPE(self)->tp_free((PyObject*) self);
}


//-----------------------------------------------------------------------------
// Subscription_Repr()
//   Return a string representation of the subscription.
//-----------------------------------------------------------------------------
static PyObject *Subscription_Repr(
    udt_Subscription *subscription)     // subscription to repr
{
    PyObject *connectionRepr, *module, *name, *result, *format, *formatArgs;

    format = cxString_FromAscii("<%s.%s on %s>");
    if (!format)
        return NULL;
    connectionRepr = PyObject_Repr((PyObject*) subscription->connection);
    if (!connectionRepr) {
        Py_DECREF(format);
        return NULL;
    }
    if (GetModuleAndName(Py_TYPE(subscription), &module, &name) < 0) {
        Py_DECREF(format);
        Py_DECREF(connectionRepr);
        return NULL;
    }
    formatArgs = PyTuple_Pack(3, module, name, connectionRepr);
    Py_DECREF(module);
    Py_DECREF(name);
    Py_DECREF(connectionRepr);
    if (!formatArgs) {
        Py_DECREF(format);
        return NULL;
    }
    result = cxString_Format(format, formatArgs);
    Py_DECREF(format);
    Py_DECREF(formatArgs);
    return result;
}


//-----------------------------------------------------------------------------
// Subscription_RegisterQuery()
//   Register a query for database change notification.
//-----------------------------------------------------------------------------
static PyObject *Subscription_RegisterQuery(
    udt_Subscription *self,             // subscription to use
    PyObject *args)                     // arguments
{
    PyObject *statement, *executeArgs;
    udt_StringBuffer statementBuffer;
    udt_Environment *env;
    udt_Cursor *cursor;
    sword status;

    // parse arguments
    executeArgs = NULL;
    if (!PyArg_ParseTuple(args, "O!|O", cxString_Type, &statement,
            &executeArgs))
        return NULL;
    if (executeArgs) {
        if (!PyDict_Check(executeArgs) && !PySequence_Check(executeArgs)) {
            PyErr_SetString(PyExc_TypeError,
                    "expecting a dictionary or sequence");
            return NULL;
        }
    }

    // create cursor to perform query
    env = self->connection->environment;
    cursor = (udt_Cursor*) Connection_NewCursor(self->connection, NULL);
    if (!cursor)
        return NULL;

    // allocate the handle so the subscription handle can be set
    if (Cursor_AllocateHandle(cursor) < 0) {
        Py_DECREF(cursor);
        return NULL;
    }

    // prepare the statement for execution
    if (StringBuffer_Fill(&statementBuffer, statement) < 0) {
        Py_DECREF(cursor);
        return NULL;
    }
    status = OCIStmtPrepare(cursor->handle, env->errorHandle,
            (text*) statementBuffer.ptr, statementBuffer.size, OCI_NTV_SYNTAX,
            OCI_DEFAULT);
    StringBuffer_Clear(&statementBuffer);
    if (Environment_CheckForError(env, status,
            "Subscription_RegisterQuery(): prepare statement") < 0) {
        Py_DECREF(cursor);
        return NULL;
    }

    // perform binds
    if (executeArgs && Cursor_SetBindVariables(cursor, executeArgs, 1, 0,
            0) < 0) {
        Py_DECREF(cursor);
        return NULL;
    }
    if (Cursor_PerformBind(cursor) < 0) {
        Py_DECREF(cursor);
        return NULL;
    }

    // parse the query in order to get the defined variables
    Py_BEGIN_ALLOW_THREADS
    status = OCIStmtExecute(self->connection->handle, cursor->handle,
            env->errorHandle, 0, 0, 0, 0, OCI_DESCRIBE_ONLY);
    Py_END_ALLOW_THREADS
    if (Environment_CheckForError(env, status,
            "Subscription_RegisterQuery(): parse statement") < 0) {
        Py_DECREF(cursor);
        return NULL;
    }

    // perform define as needed
    if (Cursor_PerformDefine(cursor) < 0) {
        Py_DECREF(cursor);
        return NULL;
    }

    // set the subscription handle
    status = OCIAttrSet(cursor->handle, OCI_HTYPE_STMT, self->handle, 0,
            OCI_ATTR_CHNF_REGHANDLE, env->errorHandle);
    if (Environment_CheckForError(env, status,
                "Subscription_RegisterQuery(): set subscription handle") < 0) {
        Py_DECREF(cursor);
        return NULL;
    }

    // execute the query which registers it
    if (Cursor_InternalExecute(cursor, 0) < 0) {
        Py_DECREF(cursor);
        return NULL;
    }
    Py_DECREF(cursor);

    Py_INCREF(Py_None);
    return Py_None;
}


//-----------------------------------------------------------------------------
// Message_Free()
//   Free the memory associated with a mesasge.
//-----------------------------------------------------------------------------
static void Message_Free(
    udt_Message *self)                  // message to free
{
    Py_CLEAR(self->dbname);
    Py_CLEAR(self->tables);
    Py_TYPE(self)->tp_free((PyObject*) self);
}
