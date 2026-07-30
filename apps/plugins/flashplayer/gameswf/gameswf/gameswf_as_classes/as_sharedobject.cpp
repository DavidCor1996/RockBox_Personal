// as_sharedobject.cpp	-- Julien Hamaide <julien.hamaide@gmail.com> 2008

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

#include "gameswf/gameswf_as_classes/as_sharedobject.h"
#include "gameswf/gameswf_as_classes/as_array.h"
#include "gameswf/gameswf_function.h"

extern "C" int flashplayer_sharedobject_read(const char *name, char *buffer,
	int buffer_size);
extern "C" int flashplayer_sharedobject_write(const char *name,
	const char *buffer, int buffer_size);
extern "C" void flashplayer_trace_sharedobject(const char *op,
	const char *name, int a, int b);

namespace gameswf
{
	struct sharedobject_field
	{
		tu_string m_name;
		as_value m_value;

		sharedobject_field()
		{
		}

		sharedobject_field(const tu_string& name, const as_value& value)
			:
			m_name(name),
			m_value(value)
		{
		}
	};

	static void append_escaped(tu_string* out, const char* src)
	{
		const char* p = src ? src : "";

		while (*p)
		{
			switch (*p)
			{
			case '\\':
				*out += "\\\\";
				break;
			case '\n':
				*out += "\\n";
				break;
			case '\r':
				*out += "\\r";
				break;
			case '\t':
				*out += "\\t";
				break;
			default:
				*out += *p;
				break;
			}
			p++;
		}
	}

	static tu_string unescape_field(const char* src, int len)
	{
		tu_string out;
		int i;

		for (i = 0; i < len && src[i]; i++)
		{
			if (src[i] == '\\' && i + 1 < len)
			{
				i++;
				switch (src[i])
				{
				case 'n':
					out += '\n';
					break;
				case 'r':
					out += '\r';
					break;
				case 't':
					out += '\t';
					break;
				default:
					out += src[i];
					break;
				}
			}
			else
			{
				out += src[i];
			}
		}

		return out;
	}

	static const char* find_tab(const char* p, const char* end)
	{
		while (p < end)
		{
			if (*p == '\t')
				return p;
			p++;
		}
		return NULL;
	}

	static void append_record(tu_string* out, char type,
		const tu_string& name, const as_value* value)
	{
		*out += type;
		*out += "\t";
		append_escaped(out, name.c_str());
		if (value)
		{
			*out += "\t";
			append_escaped(out, value->to_string());
		}
		*out += "\n";
	}

	static void serialize_object(tu_string* out, as_object* object,
		int depth, int* count)
	{
		array<sharedobject_field> fields;

		if (!object || depth > 8)
			return;

		for (stringi_hash<as_value>::const_iterator it =
			object->m_members.begin(); it != object->m_members.end(); ++it)
		{
			int array_index = -1;
			bool indexed_array_member =
				object->is(AS_ARRAY) &&
				string_to_number(&array_index, it->first.c_str()) &&
				array_index >= 0;

			/*
			 * Some older GameSWF action paths accidentally retain
			 * DONT_ENUM on values assigned into an Array.  Numeric array
			 * slots are still real persistent data and Flash SharedObject
			 * records them; omit only the Array prototype/built-in names.
			 */
			if (it->second.is_enum() || indexed_array_member)
				fields.push_back(sharedobject_field(
					it->first.c_str(), it->second));
		}

		/* Hash iteration order is an implementation detail.  Sort every
		 * object's fields so identical ActionScript state produces identical
		 * Rockbox save bytes. */
		for (int i = 0; i < fields.size(); i++)
		{
			for (int j = i + 1; j < fields.size(); j++)
			{
				if (strcmp(fields[i].m_name.c_str(),
						fields[j].m_name.c_str()) > 0)
				{
					sharedobject_field tmp = fields[i];
					fields[i] = fields[j];
					fields[j] = tmp;
				}
			}
		}

		for (int i = 0; i < fields.size(); i++)
		{
			const as_value& val = fields[i].m_value;
			if (val.is_string())
			{
				append_record(out, 'S', fields[i].m_name, &val);
				(*count)++;
			}
			else if (val.is_bool())
			{
				append_record(out, 'B', fields[i].m_name, &val);
				(*count)++;
			}
			else if (val.is_number())
			{
				append_record(out, 'N', fields[i].m_name, &val);
				(*count)++;
			}
			else if (val.is_object() && val.to_object())
			{
				as_object* child = val.to_object();
				append_record(out,
					child->is(AS_ARRAY) ? 'A' : 'O',
					fields[i].m_name, NULL);
				serialize_object(out, child, depth + 1, count);
				*out += "E\n";
				(*count)++;
			}
		}
	}

	void	as_sharedobject_getlocal(const fn_call& fn)
	{
		as_sharedobject* object =
			cast_to<as_sharedobject>(fn.this_ptr);

		*fn.result = as_sharedobject::get_local(
			fn.arg(0).to_tu_string(),
			object->get_player()).get_ptr();
	}

	void	as_sharedobject_flush(const fn_call& fn)
	{
		as_sharedobject* object =
			cast_to<as_sharedobject>(fn.this_ptr);

		fn.result->set_bool(object && object->flush());
	}

	as_sharedobject::as_sharedobject(player* player) : as_object(player)
	{
		builtin_member("getLocal", &as_sharedobject_getlocal);
		builtin_member("flush", &as_sharedobject_flush);
	}

	as_sharedobject::as_sharedobject(player* player,
		const tu_string& local_name)
		:
		as_object(player),
		m_local_name(local_name)
	{
		builtin_member("getLocal", &as_sharedobject_getlocal);
		builtin_member("flush", &as_sharedobject_flush);
		set_member("data", new as_object(player));
		load_local();
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

	gc_ptr<as_object> as_sharedobject::get_local(const tu_string& name,
		player* player)
	{
		string_hash<gc_ptr<as_object> >* local = local_objects();
		string_hash<gc_ptr<as_object> >::const_iterator it =
			local->find(name);

		if (it == local->end())
		{
			gc_ptr<as_object> new_object =
				new as_sharedobject(player, name);

			local->add(name, new_object);
			return new_object;
		}

		return it->second;
	}

	void as_sharedobject::flush_all()
	{
		string_hash<gc_ptr<as_object> >* local = local_objects();

		for (string_hash<gc_ptr<as_object> >::const_iterator it =
			local->begin(); it != local->end(); ++it)
		{
			as_sharedobject* object =
				static_cast<as_sharedobject*>(it->second.get_ptr());
			if (object)
				object->flush();
		}

		/*
		 * Flash commits local SharedObject data when a movie closes even
		 * when ActionScript never calls flush().  Stick RPG depends on that
		 * behavior.  Drop the cache while the player and its heap are still
		 * alive so a later movie cannot retain objects from this player.
		 */
		local->clear();
	}

	as_object* as_sharedobject::data_object()
	{
		as_value data;

		if (!as_object::get_member("data", &data) || !data.is_object() ||
			data.to_object() == NULL)
		{
			as_object* object = new as_object(get_player());
			set_member("data", object);
			return object;
		}

		return data.to_object();
	}

	void as_sharedobject::load_local()
	{
		static const int buffer_size = 32768;
		char* buffer;
		int got;
		const char* cursor;
		as_object* data;

		if (m_local_name.length() == 0)
			return;

		buffer = new char[buffer_size];
		if (!buffer)
			return;

		got = flashplayer_sharedobject_read(m_local_name.c_str(), buffer,
			buffer_size - 1);
		if (got <= 0)
		{
			flashplayer_trace_sharedobject("load-miss",
				m_local_name.c_str(), got, 0);
			delete [] buffer;
			return;
		}

		buffer[got] = '\0';
		cursor = buffer;
		data = data_object();

		if (strncmp(cursor, "RBSO2\n", 6) == 0)
		{
			as_object* stack[10];
			int depth = 0;
			int count = 0;

			stack[0] = data;
			cursor += 6;
			while (*cursor)
			{
				const char* line = cursor;
				const char* end = line;
				const char* key_tab;
				const char* value_tab;
				char type;
				tu_string key;
				tu_string value;
				as_value val;

				while (*end && *end != '\n')
					end++;
				if (end == line)
					goto next_v2_line;

				type = line[0];
				if (type == 'E')
				{
					if (depth > 0)
						depth--;
					goto next_v2_line;
				}
				if (end - line < 3 || line[1] != '\t')
					goto next_v2_line;

				key_tab = line + 2;
				value_tab = find_tab(key_tab, end);
				key = unescape_field(key_tab,
					(value_tab ? value_tab : end) - key_tab);

				if (type == 'O' || type == 'A')
				{
					as_object* child = type == 'A' ?
						static_cast<as_object*>(
							new as_array(get_player())) :
						new as_object(get_player());
					stack[depth]->set_member(key, child);
					if (depth + 1 < (int)(
							sizeof(stack) / sizeof(stack[0])))
						stack[++depth] = child;
					count++;
					goto next_v2_line;
				}

				if (!value_tab)
					goto next_v2_line;
				value = unescape_field(
					value_tab + 1, end - (value_tab + 1));
				if (type == 'S')
					val.set_tu_string(value);
				else if (type == 'B')
					val.set_bool(value == "1" || value == "true");
				else if (type == 'N')
				{
					double number = 0;
					string_to_number(&number, value.c_str());
					val.set_double(number);
				}
				else
					goto next_v2_line;

				stack[depth]->set_member(key, val);
				count++;

next_v2_line:
				cursor = *end == '\n' ? end + 1 : end;
			}

			flashplayer_trace_sharedobject("load", m_local_name.c_str(),
				got, count);
			delete [] buffer;
			return;
		}

		while (*cursor)
		{
			const char* line = cursor;
			const char* end = line;
			const char* name_tab;
			char type;
			tu_string key;
			tu_string value;
			as_value val;

			while (*end && *end != '\n')
				end++;

			if (end - line < 4 || line[1] != '\t')
				goto next_line;

			type = line[0];
			name_tab = find_tab(line + 2, end);
			if (!name_tab)
				goto next_line;

			key = unescape_field(line + 2, name_tab - (line + 2));
			value = unescape_field(name_tab + 1, end - (name_tab + 1));

			if (type == 'S')
			{
				val.set_tu_string(value);
			}
			else if (type == 'B')
			{
				val.set_bool(value == "1" || value == "true");
			}
			else if (type == 'N')
			{
				double number = 0;
				string_to_number(&number, value.c_str());
				val.set_double(number);
			}
			else
			{
				goto next_line;
			}

			data->set_member(key, val);

next_line:
			cursor = *end == '\n' ? end + 1 : end;
		}

		flashplayer_trace_sharedobject("load", m_local_name.c_str(), got,
			data->m_members.size());
		delete [] buffer;
	}

	bool as_sharedobject::flush()
	{
		as_object* data = data_object();
		tu_string out;
		int count = 0;
		int wrote;

		if (m_local_name.length() == 0 || !data)
			return false;

		out = "RBSO2\n";
		serialize_object(&out, data, 0, &count);

		wrote = flashplayer_sharedobject_write(m_local_name.c_str(),
			out.c_str(), out.length());
		flashplayer_trace_sharedobject("flush", m_local_name.c_str(), count,
			wrote);

		return wrote == out.length();
	}

	bool	as_sharedobject::get_member(const tu_stringi& name,
		as_value* val)
	{
		if (as_object::get_member(name, val))
		{
			return true;
		}

		as_object* object = new as_object(get_player());

		set_member(name, object);
		val->set_as_object(object);

		return true;
	}
}

// Local Variables:
// mode: C++
// c-basic-offset: 8
// tab-width: 8
// indent-tabs-mode: t
// End:
