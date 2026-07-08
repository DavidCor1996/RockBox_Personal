// gameswf_movie_def.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Some implementation for SWF player.

// Useful links:
//
// http://sswf.sourceforge.net/SWFalexref.html
// http://www.openswf.org

// @@ Need to break this file into pieces


#include "base/tu_file.h"
#include "gameswf/gameswf_font.h"
#include "gameswf/gameswf_sound.h"
#include "gameswf/gameswf_stream.h"
#include "gameswf/gameswf_fontlib.h"
#include "base/jpeg.h"
#include "base/zlib_adapter.h"

#if TU_CONFIG_LINK_TO_ZLIB
#include <zlib.h>
#endif // TU_CONFIG_LINK_TO_ZLIB


namespace gameswf
{

	// it's running in loader thread
	void movie_def_loader(void* arg)
	{
		movie_def_impl* m = (movie_def_impl*) arg;
		m->read_tags();
	}

	// Keep a table of loader functions for the different tag types.
	static loader_function s_tag_loaders[256];
	struct shared_font_entry
	{
		int id;
		font* value;
	};
	struct shared_character_entry
	{
		int id;
		character_def* value;
	};
	struct shared_sound_entry
	{
		int id;
		sound_sample* value;
	};
	struct shared_sound_export_entry
	{
		tu_stringi symbol;
		sound_sample* value;
	};
	static shared_font_entry s_shared_fonts[64];
	static int s_shared_font_count;
	static shared_character_entry s_shared_characters[4096];
	static int s_shared_character_count;
	static shared_sound_entry s_shared_sounds[4096];
	static int s_shared_sound_count;
	static shared_sound_export_entry s_shared_sound_exports[512];
	static int s_shared_sound_export_count;

	void clears_tag_loaders()
	{
		for (int i = 0; i < 256; i++)
		{
			s_tag_loaders[i] = NULL;
		}
	}

	void clear_shared_fonts()
	{
		s_shared_font_count = 0;
		s_shared_character_count = 0;
		s_shared_sound_count = 0;
		s_shared_sound_export_count = 0;
	}

	static void add_shared_font(int font_id, font* f)
	{
		for (int i = 0; i < s_shared_font_count; i++)
		{
			if (s_shared_fonts[i].id == font_id)
			{
				s_shared_fonts[i].value = f;
				return;
			}
		}
		if (s_shared_font_count < (int)(sizeof(s_shared_fonts) / sizeof(s_shared_fonts[0])))
		{
			s_shared_fonts[s_shared_font_count].id = font_id;
			s_shared_fonts[s_shared_font_count].value = f;
			s_shared_font_count++;
		}
	}

	static font* get_shared_font(int font_id)
	{
		for (int i = 0; i < s_shared_font_count; i++)
		{
			if (s_shared_fonts[i].id == font_id)
				return s_shared_fonts[i].value;
		}
		return NULL;
	}

	static void add_shared_character(int character_id, character_def* ch)
	{
		for (int i = 0; i < s_shared_character_count; i++)
		{
			if (s_shared_characters[i].id == character_id)
			{
				s_shared_characters[i].value = ch;
				return;
			}
		}
		if (s_shared_character_count < (int)(sizeof(s_shared_characters) / sizeof(s_shared_characters[0])))
		{
			s_shared_characters[s_shared_character_count].id = character_id;
			s_shared_characters[s_shared_character_count].value = ch;
			s_shared_character_count++;
		}
	}

	static character_def* get_shared_character(int character_id)
	{
		for (int i = 0; i < s_shared_character_count; i++)
		{
			if (s_shared_characters[i].id == character_id)
				return s_shared_characters[i].value;
		}
		return NULL;
	}

	static void add_shared_sound(int character_id, sound_sample* sam)
	{
		for (int i = 0; i < s_shared_sound_count; i++)
		{
			if (s_shared_sounds[i].id == character_id)
			{
				s_shared_sounds[i].value = sam;
				return;
			}
		}
		if (s_shared_sound_count < (int)(sizeof(s_shared_sounds) / sizeof(s_shared_sounds[0])))
		{
			s_shared_sounds[s_shared_sound_count].id = character_id;
			s_shared_sounds[s_shared_sound_count].value = sam;
			s_shared_sound_count++;
		}
	}

	static sound_sample* get_shared_sound(int character_id)
	{
		for (int i = 0; i < s_shared_sound_count; i++)
		{
			if (s_shared_sounds[i].id == character_id)
				return s_shared_sounds[i].value;
		}
		return NULL;
	}

	static void add_shared_sound_export(const tu_string& symbol, sound_sample* sam)
	{
		tu_stringi key(symbol);
		for (int i = 0; i < s_shared_sound_export_count; i++)
		{
			if (s_shared_sound_exports[i].symbol == key)
			{
				s_shared_sound_exports[i].value = sam;
				return;
			}
		}
		if (s_shared_sound_export_count < (int)(sizeof(s_shared_sound_exports) / sizeof(s_shared_sound_exports[0])))
		{
			s_shared_sound_exports[s_shared_sound_export_count].symbol = symbol;
			s_shared_sound_exports[s_shared_sound_export_count].value = sam;
			s_shared_sound_export_count++;
		}
	}

	sound_sample* find_shared_sound_export(const tu_string& symbol)
	{
		tu_stringi key(symbol);
		for (int i = 0; i < s_shared_sound_export_count; i++)
		{
			if (s_shared_sound_exports[i].symbol == key)
				return s_shared_sound_exports[i].value;
		}
		return NULL;
	}

	bool get_tag_loader(int tag_type, loader_function* lf)
	{
		if (tag_type < 0 || tag_type >= 256 || s_tag_loaders[tag_type] == NULL)
		{
			return false;
		}
		if (lf)
		{
			*lf = s_tag_loaders[tag_type];
		}
		return true;
	}

	void ensure_loaders_registered();

	void	register_tag_loader(int tag_type, loader_function lf)
	// Associate the specified tag type with the given tag loader
	// function.
	{
		assert(lf != NULL);

		if (tag_type >= 0 && tag_type < 256)
		{
			s_tag_loaders[tag_type] = lf;
			flashplayer_trace_loader_register(tag_type);
		}
	}

	static void	dump_tag_bytes(stream* in)
	// Log the contents of the current tag, in hex.
	{
		static const int	ROW_BYTES = 16;
		char	row_buf[ROW_BYTES];
		int	row_count = 0;

		while(in->get_position() < in->get_tag_end_position())
		{
			int	c = in->read_u8();
			log_msg("%02X", c);

			if (c < 32) c = '.';
			if (c > 127) c = '.';
			row_buf[row_count] = c;

			row_count++;
			if (row_count >= ROW_BYTES)
			{
				log_msg("    ");
				for (int i = 0; i < ROW_BYTES; i++)
				{
					log_msg("%c", row_buf[i]);
				}

				log_msg("\n");
				row_count = 0;
			}
			else
			{
				log_msg(" ");
			}
		}

		if (row_count > 0)
		{
			log_msg("\n");
		}
	}

	movie_def_impl::movie_def_impl(player* player, 
		create_bitmaps_flag cbf, create_font_shapes_flag cfs)	:
		movie_definition_sub(player),
		m_create_bitmaps(cbf),
		m_create_font_shapes(cfs),
		m_frame_rate(30.0f),
		m_version(0),
		m_loaded_length(0),
		m_jpeg_in(0),
		m_str(NULL),
		m_file_end_pos(0),
		m_zlib_in(NULL),
		m_origin_in(NULL)
	{
	}

	movie_def_impl::~movie_def_impl()
	{
		break_loading();
		if (m_thread != NULL)
		{
			m_thread->wait();
		}

		// Release our playlist data.
		{for (int i = 0, n = m_playlist.size(); i < n; i++)
		{
			for (int j = 0, m = m_playlist[i].size(); j < m; j++)
			{
				delete m_playlist[i][j];
			}
		}}

		// Release init action data.
		{for (int i = 0, n = m_init_action_list.size(); i < n; i++)
		{
			for (int j = 0, m = m_init_action_list[i].size(); j < m; j++)
			{
				delete m_init_action_list[i][j];
			}
		}}

		assert(m_jpeg_in == NULL);	// It's supposed to be cleaned up in read()
	}

	// ...
	float	movie_def_impl::get_frame_rate() const { return m_frame_rate; }
	void	movie_def_impl::set_frame_rate(float rate) { m_frame_rate = rate; }
	float	movie_def_impl::get_width_pixels() const { return ceilf(TWIPS_TO_PIXELS(m_frame_size.width())); }
	float	movie_def_impl::get_height_pixels() const { return ceilf(TWIPS_TO_PIXELS(m_frame_size.height())); }

	int	movie_def_impl::get_version() const { return m_version; }
	uint32	movie_def_impl::get_file_bytes() const { return m_file_end_pos; }
	uint32	movie_def_impl::get_loaded_bytes() const
	{
		if (m_loaded_length >= m_file_end_pos)
			return m_file_end_pos;
		return m_loaded_length;
	}

	create_bitmaps_flag	movie_def_impl::get_create_bitmaps() const
	// Returns DO_CREATE_BITMAPS if we're supposed to
	// initialize our bitmap infos, or DO_NOT_INIT_BITMAPS
	// if we're supposed to create blank placeholder
	// bitmaps (to be init'd later explicitly by the host
	// program).
	{
		return m_create_bitmaps;
	}

	create_font_shapes_flag	movie_def_impl::get_create_font_shapes() const
		// Returns DO_LOAD_FONT_SHAPES if we're supposed to
		// initialize our font shape info, or
		// DO_NOT_LOAD_FONT_SHAPES if we're supposed to not
		// create any (vector) font glyph shapes, and instead
		// rely on precached textured fonts glyphs.
	{
		return m_create_font_shapes;
	}

	void	movie_def_impl::add_bitmap_info(bitmap_info* bi)
	// All bitmap_info's used by this movie should be
	// registered with this API.
	{
		m_bitmap_list.push_back(bi);
	}


	int	movie_def_impl::get_bitmap_info_count() const { return m_bitmap_list.size(); }
	bitmap_info*	movie_def_impl::get_bitmap_info(int i) const
	{
		return m_bitmap_list[i].get_ptr();
	}

	void	movie_def_impl::export_resource(const tu_string& symbol, character_def* res)
	// Expose one of our resources under the given symbol,
	// for export.	Other movies can import it.
	{
		// SWF sometimes exports the same thing more than once!
		m_exports.set(symbol, res);
		if (sound_sample* sam = cast_to<sound_sample>(res))
		{
			add_shared_sound_export(symbol, sam);
		}
	}

	character_def*	movie_def_impl::get_exported_resource(const tu_string& symbol)
	// Get the named exported resource, if we expose it.
	// Otherwise return NULL.
	{
		gc_ptr<character_def>	res;
		m_exports.get(symbol, &res);
		return res.get_ptr();
	}

	void	movie_def_impl::add_import(const tu_string& source_url, int id, const tu_string& symbol)
	// Adds an entry to a table of resources that need to
	// be imported from other movies.  Client code must
	// call resolve_import() later, when the source movie
	// has been loaded, so that the actual resource can be
	// used.
	{
		assert(in_import_table(id) == false);

		m_imports.push_back(import_info(source_url, id, symbol));
	}

	bool	movie_def_impl::in_import_table(int character_id)
	// Debug helper; returns true if the given
	// character_id is listed in the import table.
	{
		for (int i = 0, n = m_imports.size(); i < n; i++)
		{
			if (m_imports[i].m_character_id == character_id)
			{
				return true;
			}
		}
		return false;
	}

	void	movie_def_impl::visit_imported_movies(import_visitor* visitor)
	// Calls back the visitor for each movie that we
	// import symbols from.
	{
		stringi_hash<bool>	visited;	// ugh!

		for (int i = 0, n = m_imports.size(); i < n; i++)
		{
			import_info&	inf = m_imports[i];
			if (visited.find(inf.m_source_url) == visited.end())
			{
				// Call back the visitor.
				visitor->visit(inf.m_source_url.c_str());
				visited.set(inf.m_source_url, true);
			}
		}
	}

	void	movie_def_impl::add_abc(tu_string& name, abc_def* abc)
	{
		assert(m_abc == NULL);
		m_abc = abc;
		m_abc_name = name;
	}

	void	movie_def_impl::add_symbol_class(int character_id, const tu_string& class_name)
	{
		m_symbol_class.add(character_id, class_name);
	};

	void	movie_def_impl::add_scene(int scene, const tu_string& frame_name)
	{
		m_scene.add(scene, frame_name);
	};

	void	movie_def_impl::add_frame_label(int frame, const tu_string& frame_name)
	{
		m_frame_label.add(frame, frame_name);
	};

	void	movie_def_impl::add_character(int character_id, character_def* c)
	{
		assert(c);
		c->set_id(character_id);
		m_characters.add(character_id, c);
		add_shared_character(character_id, c);
	}

	character_def*	movie_def_impl::get_character_def(int character_id)
	{
#ifndef NDEBUG
		// make sure character_id is resolved
		if (in_import_table(character_id))
		{
			log_error("get_character_def(): character_id %d is still waiting to be imported\n",
				character_id);
		}
#endif // not NDEBUG

		gc_ptr<character_def>	ch;
		m_characters.get(character_id, &ch);
		if (ch == NULL)
		{
			gc_ptr<bitmap_character_def> bitmap;
			m_bitmap_characters.get(character_id, &bitmap);
			ch = bitmap.get_ptr();
		}
		if (ch == NULL)
		{
			ch = get_shared_character(character_id);
		}
		assert(ch == NULL || gc_collector::debug_get_ref_count(ch) > 1);
		return ch.get_ptr();
	}

	bool	movie_def_impl::get_labeled_frame(const char* label, int* frame_number)
		// Returns 0-based frame #
	{
		return m_named_frames.get(label, frame_number);
	}

	void	movie_def_impl::add_font(int font_id, font* f)
	{
		assert(f);
		m_fonts.add(font_id, f);
		add_shared_font(font_id, f);
		flashplayer_trace_font(0, font_id);
	}

	font*	movie_def_impl::get_font(int font_id)
	{
#ifndef NDEBUG
		// make sure font_id is resolved
		if (in_import_table(font_id))
		{
			log_error("get_font(): font_id %d is still waiting to be imported\n",
				font_id);
		}
#endif // not NDEBUG

		gc_ptr<font>	f;
		m_fonts.get(font_id, &f);
		if (f == NULL)
		{
			font* shared = get_shared_font(font_id);
			if (shared)
				f = shared;
			flashplayer_trace_font(shared == NULL ? 3 : 2, font_id);
		}
		else
		{
			flashplayer_trace_font(1, font_id);
		}
		assert(f == NULL || gc_collector::debug_get_ref_count(f) > 1);
		return f.get_ptr();
	}

	font*	movie_def_impl::find_font(const char* name) const
	{
		for (hash<int, gc_ptr<font> >::const_iterator it = m_fonts.begin(); it != m_fonts.end(); ++it)
		{
			if (it->second->get_name() == name)
			{
				return it->second.get_ptr();
			}
		}
		return NULL;
	}

	bitmap_character_def*	movie_def_impl::get_bitmap_character(int character_id)
	{
		gc_ptr<bitmap_character_def>	ch;
		m_bitmap_characters.get(character_id, &ch);
		assert(ch == NULL || gc_collector::debug_get_ref_count(ch) > 1);
		return ch.get_ptr();
	}

	void	movie_def_impl::add_bitmap_character(int character_id, bitmap_character_def* ch)
	{
		assert(ch);
		ch->set_id(character_id);
		m_bitmap_characters.add(character_id, ch);
		add_shared_character(character_id, ch);

		add_bitmap_info(ch->get_bitmap_info());
	}

	sound_sample*	movie_def_impl::get_sound_sample(int character_id)
	{
		gc_ptr<sound_sample>	ch;
		m_sound_samples.get(character_id, &ch);
		if (ch == NULL)
		{
			sound_sample* shared = get_shared_sound(character_id);
			if (shared)
				ch = shared;
		}
		assert(ch == NULL || gc_collector::debug_get_ref_count(ch) > 1);
		return ch.get_ptr();
	}

	void	movie_def_impl::add_sound_sample(int character_id, sound_sample* sam)
	{
		assert(sam);
		m_sound_samples.add(character_id, sam);
		add_shared_sound(character_id, sam);

		for (int i = 0, n = m_pending_sound_exports.size(); i < n; i++)
		{
			if (m_pending_sound_exports[i].m_character_id == character_id)
			{
				export_resource(m_pending_sound_exports[i].m_symbol, sam);
			}
		}
	}

	void	movie_def_impl::add_pending_sound_export(int character_id, const tu_string& symbol)
	{
		for (int i = 0, n = m_pending_sound_exports.size(); i < n; i++)
		{
			if (m_pending_sound_exports[i].m_character_id == character_id
				&& m_pending_sound_exports[i].m_symbol == symbol)
			{
				return;
			}
		}
		m_pending_sound_exports.push_back(pending_sound_export(character_id, symbol));
	}

	void	movie_def_impl::add_execute_tag(execute_tag* e)
	{
		assert(e);
		flashplayer_trace_execute_tag(get_loading_frame());
		m_playlist[get_loading_frame()].push_back(e);
	}

	void	movie_def_impl::add_init_action(int sprite_id, execute_tag* e)
	// Need to execute the given tag before entering the
	// currently-loading frame for the first time.
	//
	// @@ AFAIK, the sprite_id is totally pointless -- correct?
	{
		assert(e);
		m_init_action_list[get_loading_frame()].push_back(e);
	}

	void	movie_def_impl::add_frame_name(const char* name)
	// Labels the frame currently being loaded with the
	// given name.	A copy of the name string is made and
	// kept in this object.
	{
		tu_string	n = name;
		assert(m_named_frames.get(n, NULL) == false);	// frame should not already have a name (?)
		m_named_frames.add(n, get_loading_frame());	// stores 0-based frame #
	}

	void	movie_def_impl::set_jpeg_loader(jpeg::input* j_in)
	// Set an input object for later loading DefineBits
	// images (JPEG images without the table info).
	{
		assert(m_jpeg_in == NULL);
		m_jpeg_in = j_in;
	}

	jpeg::input*	movie_def_impl::get_jpeg_loader()
	// Get the jpeg input loader, to load a DefineBits
	// image (one without table info).
	{
		return m_jpeg_in;
	}


	const array<execute_tag*>&	movie_def_impl::get_playlist(int frame_number) { return m_playlist[frame_number]; }

	const array<execute_tag*>*	movie_def_impl::get_init_actions(int frame_number)
	{
		return &m_init_action_list[frame_number];
	}

	void	movie_def_impl::read(tu_file* in)
	// Read a .SWF movie.
	{
		m_origin_in = in;
		Uint32	file_start_pos = in->get_position();
		Uint32	header = in->read_le32();
		Uint32	file_length = in->read_le32();
		m_file_end_pos = file_start_pos + file_length;

		m_version = (header >> 24) & 255;
		if ((header & 0x0FFFFFF) != 0x00535746
			&& (header & 0x0FFFFFF) != 0x00535743)
		{
			// ERROR
			log_error("gameswf::movie_def_impl::read() -- file does not start with a SWF header!\n");
			return;
		}
		bool	compressed = (header & 255) == 'C';

		IF_VERBOSE_PARSE(log_msg("version = %d, file_length = %d\n", m_version, file_length));

		m_zlib_in = NULL;
		if (compressed)
		{
#if TU_CONFIG_LINK_TO_ZLIB == 0
			log_error("movie_def_impl::read(): unable to read zipped SWF data; TU_CONFIG_LINK_TO_ZLIB is 0\n");
			return;
#endif

			IF_VERBOSE_PARSE(log_msg("file is compressed.\n"));

			// Uncompress the input as we read it.
			in = zlib_adapter::make_inflater(in);
			m_zlib_in = in;

			// Subtract the size of the 8-byte header, since
			// it's not included in the compressed
			// stream length.
			m_file_end_pos = file_length - 8;
		}

		m_str = new stream(in);

		m_frame_size.read(m_str);
		m_frame_rate = m_str->read_u16() / 256.0f;

		set_frame_count(m_str->read_u16());

		m_playlist.resize(get_frame_count());
		m_init_action_list.resize(get_frame_count());

		IF_VERBOSE_PARSE(m_frame_size.print());
		IF_VERBOSE_PARSE(log_msg("frame rate = %f, frames = %d\n", m_frame_rate, get_frame_count()));

		if (get_player()->use_separate_thread())
		{
			// if you want to use multithread movie loader
			m_thread = new tu_thread(movie_def_loader, this);
		}
		else
		{
			// if you does not want to use multithread movie loader
			read_tags();
		}
	}

	// is running in loader thread
	void	movie_def_impl::read_tags()
	{
		int tag_count = 0;
		int last_pos = -1;

		ensure_loaders_registered();

		while ((Uint32) m_str->get_position() < m_file_end_pos && get_break_loading() == false)
		{
			int start_pos = m_str->get_position();
			int	tag_type = m_str->open_tag();
			loader_function	lf = NULL;
			tag_count++;
			flashplayer_trace_tag(tag_count, tag_type, start_pos);
			flashplayer_trace_parse_start("movie", tag_count, tag_type,
				start_pos);
			//IF_VERBOSE_PARSE(log_msg("tag_type = %d\n", tag_type));
			if (flashplayer_should_skip_movie_tag(get_loading_frame(),
				tag_count, tag_type, start_pos))
			{
				flashplayer_trace_loader(0, tag_type);
			}
			else if (tag_type == 1)
			{
				// show frame tag -- advance to the next frame.
				IF_VERBOSE_PARSE(log_msg("  show_frame\n"));
				inc_loading_frame();
			}
			else
			if (get_tag_loader(tag_type, &lf))
			{
				flashplayer_trace_loader(1, tag_type);
				// call the tag loader.	 The tag loader should add
				// characters or tags to the movie data structure.
				(*lf)(m_str, tag_type, this);

			}
			else
			{
				flashplayer_trace_loader(0, tag_type);
				// no tag loader for this tag type.
				log_msg("*** no tag loader for type %d\n", tag_type);
				IF_VERBOSE_PARSE(dump_tag_bytes(m_str));
			}

				m_str->close_tag();
				if (m_str->get_position() <= last_pos)
				{
					flashplayer_trace_movie_state("read_loop_stall",
						tag_type, tag_count, m_str->get_position());
					log_msg("warning: loader stopped advancing at tag %d type %d\n",
						tag_count, tag_type);
					break;
				}
				last_pos = m_str->get_position();
				flashplayer_trace_parse_end("movie", tag_count,
					tag_type, last_pos);
				flashplayer_trace_parse_progress("movie", tag_count,
					tag_type, last_pos);

				if (flashplayer_should_stop_movie_load(get_loading_frame(),
					tag_count, last_pos))
				{
					flashplayer_trace_movie_state("read_loop_fast_stop",
						get_loading_frame(), tag_count, last_pos);
					log_msg("flashplayer: stopping movie load at frame %d tag %d pos %d\n",
						get_loading_frame(), tag_count, last_pos);
					break;
				}

				if (tag_type == 0)
				{
				if ((unsigned int) m_str->get_position() != m_file_end_pos)
				{
					flashplayer_trace_movie_state("read_loop_early_end",
						tag_type, tag_count, m_str->get_position());
					// Safety break, so we don't read past the end of the
					// movie.
					log_msg("warning: hit stream-end tag, but not at the "
						"end of the file yet; stopping for safety\n");
					break;
				}
			}

			m_loaded_length = m_str->get_position();
		}
		flashplayer_trace_movie_state("read_loop_done",
			get_loading_frame(), tag_count, m_str->get_position());
		if (m_str->get_position() >= m_file_end_pos)
		{
			m_loaded_length = m_file_end_pos;
		}

		if (m_jpeg_in)
		{
			delete m_jpeg_in;
			m_jpeg_in = NULL;
		}

		if (m_zlib_in)
		{
			// Done with the zlib_adapter.
			delete m_zlib_in;
		}

		delete m_str;
		delete m_origin_in;
	}


	void	movie_def_impl::get_owned_fonts(array<font*>* fonts)
	// Fill up *fonts with fonts that we own.
	{
		assert(fonts);
		fonts->resize(0);

		array<int>	font_ids;

		for (hash<int, gc_ptr<font> >::iterator it = m_fonts.begin();
			it != m_fonts.end();
			++it)
		{
			font*	f = it->second.get_ptr();
			if (f->get_owning_movie() == this)
			{
				// Sort by character id, so the ordering is
				// consistent for cache read/write.
				int	id = it->first;

				// Insert in correct place.
				int	insert;
				for (insert = 0; insert < font_ids.size(); insert++)
				{
					if (font_ids[insert] > id)
					{
						// We want to insert here.
						break;
					}
				}
				fonts->insert(insert, f);
				font_ids.insert(insert, id);
			}
		}
	}

	// Increment this when the cache data format changes.
	#define CACHE_FILE_VERSION 7

	void	movie_def_impl::output_cached_data(tu_file* out, const cache_options& options)
	// Dump our cached data into the given stream.
	{
		// Write a little header.
		char	header[5];
		strcpy(header, "gscX");
		header[3] = CACHE_FILE_VERSION;
		compiler_assert(CACHE_FILE_VERSION < 256);

		out->write_bytes(header, 4);

		// Write font data.
//		array<font*>	fonts;
//		get_owned_fonts(&fonts);
//		fontlib::output_cached_data(out, fonts, this, options);

		// Write character data.
		{for (hash<int, gc_ptr<character_def> >::iterator it = m_characters.begin();
		it != m_characters.end();
		++it)
		{
			tu_file	char_cache(tu_file::memory_buffer);

			it->second->output_cached_data(&char_cache, options);
			out->write_le16(it->first);
			out->write_le32(char_cache.get_position());
			char_cache.set_position(0);
			out->copy_bytes(&char_cache, char_cache.size());
		}}

		out->write_le16(static_cast<uint16>(-1));	// end of characters marker
	}

	void	movie_def_impl::input_cached_data(tu_file* in)
	// Read in cached data and use it to prime our loaded characters.
	{
		// Read the header & check version.
		unsigned char	header[4];
		in->read_bytes(header, 4);
		if (header[0] != 'g' || header[1] != 's' || header[2] != 'c')
		{
			log_error("cache file does not have the correct format; skipping\n");
			return;
		}
		else if (header[3] != CACHE_FILE_VERSION)
		{
			log_error(
				"cached data is version %d, but we require version %d; skipping\n",
				int(header[3]), CACHE_FILE_VERSION);
			return;
		}

		// Read the cached font data.
//		array<font*>	fonts;
//		get_owned_fonts(&fonts);
//		fontlib::input_cached_data(in, fonts, this);

		// Read the cached character data.
		for (;;)
		{
			if (in->get_error() != TU_FILE_NO_ERROR)
			{
				log_error("error reading cache file (characters); skipping\n");
				return;
			}
			if (in->get_eof())
			{
				log_error("unexpected eof reading cache file (characters); skipping\n");
				return;
			}

			Sint16	id = in->read_le16();
			if (id == (Sint16) -1) { break; }	// done
			int cached_size = in->read_le32();
			int cached_end = in->get_position() + cached_size;

			gc_ptr<character_def> ch;
			m_characters.get(id, &ch);
			if (ch != NULL)
			{
				ch->input_cached_data(in);
				if (in->get_position() != cached_end)
				{
					in->set_position(cached_end);
				}
			}
			else
			{
				in->set_position(cached_end);
			}
		}
	}

	// flash9
	as_function* movie_def_impl::instanciate_class(character* ch) const
	{
		// If the character ID is zero, the class is associated with the main timeline 
		// -1 means _root
		int symbol_id = ch->get_id();
		if (symbol_id == -1)
		{
			symbol_id = 0;
		}

		tu_string class_name;
		if (m_symbol_class.get(symbol_id, &class_name))
		{
			assert(m_abc != NULL);

			instance_info* ii = m_abc->get_instance_info(class_name);

			ch->set_instance( ii );

			// create traits
			for (int i = 0; i < ii->m_trait.size(); i++)
			{
				traits_info* ti = ii->m_trait[i].get();
				const char* name = m_abc->get_multiname(ti->m_name);
				as_value val;
				switch (ti->m_kind)
				{
					case traits_info::Trait_Const:
					case traits_info::Trait_Slot:
						//TODO
						val = 0;
						break;

					case traits_info::Trait_Getter:
					case traits_info::Trait_Setter:
					case traits_info::Trait_Method:
					{
						int index = ti->trait_method.m_method;
						val.set_as_object(m_abc->m_method[index].get());
						break;
					}

					case traits_info::Trait_Class:
					case traits_info::Trait_Function:
						assert(0&&"todo");
						break;

					default:
						assert(0);
				}

				ch->set_member(name, val);
			}

			// get function3 from abc
			return m_abc->get_class_constructor(class_name);
		}
		return NULL;
	}

}
