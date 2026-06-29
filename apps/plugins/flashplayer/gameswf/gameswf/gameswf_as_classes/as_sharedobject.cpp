// as_sharedobject.cpp	-- Julien Hamaide <julien.hamaide@gmail.com> 2008

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

#include "gameswf/gameswf_as_classes/as_sharedobject.h"
#include "gameswf/gameswf_function.h"

namespace gameswf
{
	void	as_sharedobject_getlocal(const fn_call& fn)
	{
		as_sharedobject* object = cast_to<as_sharedobject>(fn.this_ptr);

		*fn.result = as_sharedobject::get_local( fn.arg( 0 ).to_tu_string(), object->get_player() ).get_ptr();
	}

	void	as_sharedobject_flush(const fn_call& fn)
	{
		//todo
	}

	as_sharedobject::as_sharedobject( player * player ) : as_object( player )
	{
		builtin_member( "getLocal", &as_sharedobject_getlocal );
		builtin_member( "flush", &as_sharedobject_flush );
	}

	string_hash<gc_ptr<as_object> >* as_sharedobject::local_objects()
	{
		static string_hash<gc_ptr<as_object> >* local;
		if (local == NULL)
		{
			local = new string_hash<gc_ptr<as_object> >;
		}
		return local;
	}

	gc_ptr<as_object> as_sharedobject::get_local( const tu_string & name, player * player )
	{
		string_hash<gc_ptr<as_object> >* local = local_objects();
		string_hash<gc_ptr<as_object> >::const_iterator it = local->find( name );

		if( it == local->end() )
		{
			gc_ptr<as_object> new_object = new as_sharedobject( player );

			local->add( name, new_object );
			return new_object;
		}

		return it->second;
	}

	bool	as_sharedobject::get_member(const tu_stringi& name, as_value* val)
	{
		if( as_object::get_member( name, val ) )
		{
			return true;
		}

		as_object * object = new as_object( get_player() );

		set_member( name, object );
		val->set_as_object( object );

		return true;
	}
}

// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
