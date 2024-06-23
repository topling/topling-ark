template<class ThisType>
class s_arg_packet<rpc_return_t (ThisType::*)()>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)();
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
		);
	}
};

template<class ThisType, class Arg0>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0)
		);
	}
};

template<class ThisType, class Arg0, class Arg1>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5),
			ArgVal<Arg6>::deref(argvals.a6)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5),
			ArgVal<Arg6>::deref(argvals.a6),
			ArgVal<Arg7>::deref(argvals.a7)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5),
			ArgVal<Arg6>::deref(argvals.a6),
			ArgVal<Arg7>::deref(argvals.a7),
			ArgVal<Arg8>::deref(argvals.a8)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8, class Arg9>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5),
			ArgVal<Arg6>::deref(argvals.a6),
			ArgVal<Arg7>::deref(argvals.a7),
			ArgVal<Arg8>::deref(argvals.a8),
			ArgVal<Arg9>::deref(argvals.a9)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8, class Arg9, class Arg10>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5),
			ArgVal<Arg6>::deref(argvals.a6),
			ArgVal<Arg7>::deref(argvals.a7),
			ArgVal<Arg8>::deref(argvals.a8),
			ArgVal<Arg9>::deref(argvals.a9),
			ArgVal<Arg10>::deref(argvals.a10)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8, class Arg9, class Arg10, class Arg11>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5),
			ArgVal<Arg6>::deref(argvals.a6),
			ArgVal<Arg7>::deref(argvals.a7),
			ArgVal<Arg8>::deref(argvals.a8),
			ArgVal<Arg9>::deref(argvals.a9),
			ArgVal<Arg10>::deref(argvals.a10),
			ArgVal<Arg11>::deref(argvals.a11)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8, class Arg9, class Arg10, class Arg11, class Arg12>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5),
			ArgVal<Arg6>::deref(argvals.a6),
			ArgVal<Arg7>::deref(argvals.a7),
			ArgVal<Arg8>::deref(argvals.a8),
			ArgVal<Arg9>::deref(argvals.a9),
			ArgVal<Arg10>::deref(argvals.a10),
			ArgVal<Arg11>::deref(argvals.a11),
			ArgVal<Arg12>::deref(argvals.a12)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8, class Arg9, class Arg10, class Arg11, class Arg12, class Arg13>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5),
			ArgVal<Arg6>::deref(argvals.a6),
			ArgVal<Arg7>::deref(argvals.a7),
			ArgVal<Arg8>::deref(argvals.a8),
			ArgVal<Arg9>::deref(argvals.a9),
			ArgVal<Arg10>::deref(argvals.a10),
			ArgVal<Arg11>::deref(argvals.a11),
			ArgVal<Arg12>::deref(argvals.a12),
			ArgVal<Arg13>::deref(argvals.a13)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8, class Arg9, class Arg10, class Arg11, class Arg12, class Arg13, class Arg14>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5),
			ArgVal<Arg6>::deref(argvals.a6),
			ArgVal<Arg7>::deref(argvals.a7),
			ArgVal<Arg8>::deref(argvals.a8),
			ArgVal<Arg9>::deref(argvals.a9),
			ArgVal<Arg10>::deref(argvals.a10),
			ArgVal<Arg11>::deref(argvals.a11),
			ArgVal<Arg12>::deref(argvals.a12),
			ArgVal<Arg13>::deref(argvals.a13),
			ArgVal<Arg14>::deref(argvals.a14)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8, class Arg9, class Arg10, class Arg11, class Arg12, class Arg13, class Arg14, class Arg15>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5),
			ArgVal<Arg6>::deref(argvals.a6),
			ArgVal<Arg7>::deref(argvals.a7),
			ArgVal<Arg8>::deref(argvals.a8),
			ArgVal<Arg9>::deref(argvals.a9),
			ArgVal<Arg10>::deref(argvals.a10),
			ArgVal<Arg11>::deref(argvals.a11),
			ArgVal<Arg12>::deref(argvals.a12),
			ArgVal<Arg13>::deref(argvals.a13),
			ArgVal<Arg14>::deref(argvals.a14),
			ArgVal<Arg15>::deref(argvals.a15)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8, class Arg9, class Arg10, class Arg11, class Arg12, class Arg13, class Arg14, class Arg15, class Arg16>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5),
			ArgVal<Arg6>::deref(argvals.a6),
			ArgVal<Arg7>::deref(argvals.a7),
			ArgVal<Arg8>::deref(argvals.a8),
			ArgVal<Arg9>::deref(argvals.a9),
			ArgVal<Arg10>::deref(argvals.a10),
			ArgVal<Arg11>::deref(argvals.a11),
			ArgVal<Arg12>::deref(argvals.a12),
			ArgVal<Arg13>::deref(argvals.a13),
			ArgVal<Arg14>::deref(argvals.a14),
			ArgVal<Arg15>::deref(argvals.a15),
			ArgVal<Arg16>::deref(argvals.a16)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8, class Arg9, class Arg10, class Arg11, class Arg12, class Arg13, class Arg14, class Arg15, class Arg16, class Arg17>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16, Arg17)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16, Arg17);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5),
			ArgVal<Arg6>::deref(argvals.a6),
			ArgVal<Arg7>::deref(argvals.a7),
			ArgVal<Arg8>::deref(argvals.a8),
			ArgVal<Arg9>::deref(argvals.a9),
			ArgVal<Arg10>::deref(argvals.a10),
			ArgVal<Arg11>::deref(argvals.a11),
			ArgVal<Arg12>::deref(argvals.a12),
			ArgVal<Arg13>::deref(argvals.a13),
			ArgVal<Arg14>::deref(argvals.a14),
			ArgVal<Arg15>::deref(argvals.a15),
			ArgVal<Arg16>::deref(argvals.a16),
			ArgVal<Arg17>::deref(argvals.a17)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8, class Arg9, class Arg10, class Arg11, class Arg12, class Arg13, class Arg14, class Arg15, class Arg16, class Arg17, class Arg18>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16, Arg17, Arg18)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16, Arg17, Arg18);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5),
			ArgVal<Arg6>::deref(argvals.a6),
			ArgVal<Arg7>::deref(argvals.a7),
			ArgVal<Arg8>::deref(argvals.a8),
			ArgVal<Arg9>::deref(argvals.a9),
			ArgVal<Arg10>::deref(argvals.a10),
			ArgVal<Arg11>::deref(argvals.a11),
			ArgVal<Arg12>::deref(argvals.a12),
			ArgVal<Arg13>::deref(argvals.a13),
			ArgVal<Arg14>::deref(argvals.a14),
			ArgVal<Arg15>::deref(argvals.a15),
			ArgVal<Arg16>::deref(argvals.a16),
			ArgVal<Arg17>::deref(argvals.a17),
			ArgVal<Arg18>::deref(argvals.a18)
		);
	}
};

template<class ThisType, class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8, class Arg9, class Arg10, class Arg11, class Arg12, class Arg13, class Arg14, class Arg15, class Arg16, class Arg17, class Arg18, class Arg19>
class s_arg_packet<rpc_return_t (ThisType::*)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16, Arg17, Arg18, Arg19)>
	: public s_arg_packet_base
{
public:
	typedef rpc_return_t (ThisType::*function_t)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9, Arg10, Arg11, Arg12, Arg13, Arg14, Arg15, Arg16, Arg17, Arg18, Arg19);
	typedef arglist_val<function_t> val_t;
	typedef arglist_ref<function_t> ref_t;
	aval_t argvals;
	void invoke_f(function_t fun)
	{
		this->retv = (argvals.self->*fun)(
			ArgVal<Arg0>::deref(argvals.a0),
			ArgVal<Arg1>::deref(argvals.a1),
			ArgVal<Arg2>::deref(argvals.a2),
			ArgVal<Arg3>::deref(argvals.a3),
			ArgVal<Arg4>::deref(argvals.a4),
			ArgVal<Arg5>::deref(argvals.a5),
			ArgVal<Arg6>::deref(argvals.a6),
			ArgVal<Arg7>::deref(argvals.a7),
			ArgVal<Arg8>::deref(argvals.a8),
			ArgVal<Arg9>::deref(argvals.a9),
			ArgVal<Arg10>::deref(argvals.a10),
			ArgVal<Arg11>::deref(argvals.a11),
			ArgVal<Arg12>::deref(argvals.a12),
			ArgVal<Arg13>::deref(argvals.a13),
			ArgVal<Arg14>::deref(argvals.a14),
			ArgVal<Arg15>::deref(argvals.a15),
			ArgVal<Arg16>::deref(argvals.a16),
			ArgVal<Arg17>::deref(argvals.a17),
			ArgVal<Arg18>::deref(argvals.a18),
			ArgVal<Arg19>::deref(argvals.a19)
		);
	}
};

