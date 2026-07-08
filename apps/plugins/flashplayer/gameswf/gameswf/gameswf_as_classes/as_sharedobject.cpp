// as_sharedobject.cpp	-- Julien Hamaide <julien.hamaide@gmail.com> 2008

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

#include "gameswf/gameswf_as_classes/as_sharedobject.h"
#include "gameswf/gameswf_function.h"

extern "C" int flashplayer_sharedobject_read(const char *name, char *buffer,
	int buffer_size);
extern "C" int flashplayer_sharedobject_write(const char *name,
	const char *buffer, int buffer_size);
extern "C" void flashplayer_trace_sharedobject(const char *op,
	const char *name, int a, int b);

namespace gameswf
{
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
		char buffer[4096];
		int got;
		const char* cursor;
		as_object* data;

		if (m_local_name.length() == 0)
			return;

		got = flashplayer_sharedobject_read(m_local_name.c_str(), buffer,
			sizeof(buffer) - 1);
		if (got <= 0)
		{
			flashplayer_trace_sharedobject("load-miss",
				m_local_name.c_str(), got, 0);
			return;
		}

		buffer[got] = '\0';
		cursor = buffer;
		data = data_object();

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
	}

	bool as_sharedobject::flush()
	{
		as_object* data = data_object();
		tu_string out;
		int count = 0;
		int wrote;

		if (m_local_name.length() == 0 || !data)
			return false;

		out = "RBSO1\n";

		for (stringi_hash<as_value>::const_iterator it =
			data->m_members.begin(); it != data->m_members.end(); ++it)
		{
			const as_value& val = it->second;

			if (val.is_string())
				out += "S\t";
			else if (val.is_bool())
				out += "B\t";
			else if (val.is_number())
				out += "N\t";
			else
				continue;

			append_escaped(&out, it->first.c_str());
			out += "\t";
			append_escaped(&out, val.to_string());
			out += "\n";
			count++;
		}

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
