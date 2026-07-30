// as_sharedobject.h	-- Julien Hamaide <julien.hamaide@gmail.com> 2008

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Action Script SharedObject implementation code for the gameswf SWF player library.


#ifndef GAMESWF_AS_SHAREOBJECT_H
#define GAMESWF_AS_SHAREOBJECT_H

#include "gameswf/gameswf_action.h"	// for as_object

namespace gameswf
{

	class as_sharedobject : public as_object
	{
		static string_hash<gc_ptr<as_object> >* local_objects();
		tu_string m_local_name;

		as_object* data_object();
		void load_local();

	public:

		as_sharedobject( player * player );
		as_sharedobject( player * player, const tu_string& local_name );

		bool	get_member(const tu_stringi& name, as_value* val);
		bool	flush();

		static gc_ptr<as_object> get_local( const tu_string & name, player * player );
		static void flush_all();

	};

}

#endif //GAMESWF_AS_SHAREOBJECT_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
