// as_sound.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Action Script Array implementation code for the gameswf SWF player library.


#ifndef GAMESWF_AS_SOUND_H
#define GAMESWF_AS_SOUND_H

#include "gameswf/gameswf_action.h"	// for as_object

namespace gameswf
{

	void	as_global_sound_ctor(const fn_call& fn);
	bool	call_null_sound_compat(const tu_string& varname,
		const tu_string& method_name, as_environment* env, int nargs,
		int first_arg_bottom_index, as_value* result);

	struct as_sound : public as_object
	{

		// Unique id of a gameswf resource
		enum { m_class_id = AS_SOUND };
		virtual bool is(int class_id) const
		{
			if (m_class_id == class_id) return true;
			else return as_object::is(class_id);
		}

		as_sound(player* player) :
			as_object(player),
			m_id(-1),
			m_is_loaded_sound(false)
		{
		}

		~as_sound()
		{
			clear();
		}

		void clear()
		{
			if (m_is_loaded_sound && m_id >= 0)
			{
				sound_handler* sh = get_sound_handler();
				if (sh)
				{
					sh->delete_sound(m_id);
				}
			}
			m_is_loaded_sound = false;
			m_id = -1;
		}

		virtual bool get_member(const tu_stringi& name, as_value* val);

		// id of the sound
		int m_id;

		bool m_is_loaded_sound;
		weak_ptr<character> m_target;
	};

}	// end namespace gameswf


#endif // GAMESWF_AS_SOUND_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
