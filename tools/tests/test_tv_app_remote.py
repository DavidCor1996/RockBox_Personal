#!/usr/bin/env python3
"""Run production app adapters through production TV remote keymaps."""
from pathlib import Path
import runpy,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
base=runpy.run_path(str(root/'tools/tests/test_tv_remote_navigation.py'))
lib=(root/'apps/plugins/lib/pluginlib_actions.c').read_text()
lib=lib[lib.index('static bool browser_remote;'):]
live=(root/'apps/plugins/mpegplayer/livetv_guide.c').read_text();a=live.index('static int livetv_remote_button(');b=live.index('static void livetv_parental_draw_pin',a);live=live[a:b]
for name in ['youtube','twitch']:
 text=(root/f'apps/plugins/{name}.c').read_text()
 assert 'action = pluginlib_getaction_remote(' in text
 assert 'case ACTION_STD_MENU:' in text
 home=text.split('case ACTION_STD_MENU:')[1].split('case PLA_CANCEL:')[0]
 assert ('running = false;' in home) if name=='youtube' else ('tw_restore_lcd();' in home and 'return status;' in home)
extra=r'''
#define CONTEXT_PLUGIN (1<<21)
#define ACTION_REMOTE (1<<28)
enum {PLA_UP=1000,PLA_DOWN,PLA_LEFT,PLA_RIGHT,PLA_UP_REPEAT,PLA_DOWN_REPEAT,
 PLA_SELECT_REL,PLA_SELECT_REPEAT,PLA_CANCEL,PLA_EXIT};
#define LIVETV_BTN_UP 0x101
#define LIVETV_BTN_DOWN 0x102
#define LIVETV_BTN_LEFT 0x103
#define LIVETV_BTN_RIGHT 0x104
#define LIVETV_BTN_SELECT 0x105
#define LIVETV_BTN_EXIT 0x106
#define LIVETV_BTN_EXIT_REL 0x107
static struct button_mapping **plugin_context_order;
static int plugin_context_count,last_context;
static bool remote_input=true;
static unsigned app_button,app_previous;
static const struct button_mapping local_map[]={LAST_ITEM_IN_LIST};
static const struct button_mapping *get_context_map(int context)
{assert(!(context&CONTEXT_REMOTE));assert(plugin_context_count==1);return plugin_context_order[last_context++];}
static int app_custom(int context,int timeout,const struct button_mapping*(*cb)(int))
{
 assert(context==CONTEXT_PLUGIN && timeout==20);
 const struct button_mapping *m=cb(context|(remote_input?CONTEXT_REMOTE:0));
 if(!remote_input){assert(m==local_map);return PLA_RIGHT;}
 assert(m->action_code==-CONTEXT_MAINMENU-100);
 return lookup(CONTEXT_MAINMENU,app_button,app_previous);
}
static int app_status(int*p){if(p)*p=app_button;return remote_input?ACTION_REMOTE:0;}
static struct {int(*get_custom_action)(int,int,const struct button_mapping*(*)(int));int(*get_action_statuscode)(int*);} api={app_custom,app_status};
#define rb (&api)
'''+lib+'\n'+live+r'''
static int app_action(unsigned key,unsigned previous)
{
 const struct button_mapping *maps[]={local_map};app_button=key;app_previous=previous;
 return pluginlib_getaction_remote(20,maps,1);
}
static void check_app_remote(void)
{
 assert(app_action(BUTTON_RC_VOL_UP,0)==PLA_UP);
 assert(app_action(BUTTON_RC_VOL_DOWN,0)==PLA_DOWN);
 assert(app_action(BUTTON_RC_UP|BUTTON_REPEAT,BUTTON_RC_UP)==PLA_UP_REPEAT);
 assert(app_action(BUTTON_RC_DOWN|BUTTON_REPEAT,BUTTON_RC_DOWN)==PLA_DOWN_REPEAT);
 assert(app_action(BUTTON_RC_LEFT|BUTTON_REL,BUTTON_RC_LEFT)==PLA_LEFT);
 assert(app_action(BUTTON_RC_RIGHT|BUTTON_REPEAT,BUTTON_RC_RIGHT)==PLA_RIGHT);
 assert(app_action(BUTTON_RC_PLAY|BUTTON_REL,BUTTON_RC_PLAY)==PLA_SELECT_REL);
 assert(app_action(BUTTON_RC_SELECT|BUTTON_REL,BUTTON_RC_SELECT)==PLA_SELECT_REL);
 assert(app_action(BUTTON_RC_MENU|BUTTON_REL,BUTTON_RC_MENU)==PLA_CANCEL);
 assert(app_action(BUTTON_RC_MENU|BUTTON_REPEAT,BUTTON_RC_MENU)==ACTION_STD_MENU);
 assert(app_action(BUTTON_RC_MENU|BUTTON_REL,BUTTON_RC_MENU|BUTTON_REPEAT)==ACTION_NONE);
 assert(app_action(BUTTON_RC_LEFT|BUTTON_REPEAT,BUTTON_RC_LEFT)==PLA_CANCEL);
 remote_input=false;assert(app_action(0,0)==PLA_RIGHT);remote_input=true;
 active=false;assert(app_action(BUTTON_RC_VOL_UP,0)==ACTION_NONE);active=true;
 kokkia=true;assert(app_action(BUTTON_RC_VOL_UP,0)==ACTION_NONE);kokkia=false;
 assert(livetv_remote_button(BUTTON_RC_VOL_UP)==LIVETV_BTN_UP);
 assert(livetv_remote_button(BUTTON_RC_VOL_DOWN|BUTTON_REPEAT)==(LIVETV_BTN_DOWN|BUTTON_REPEAT));
 assert(livetv_remote_button(BUTTON_RC_VOL_DOWN|BUTTON_REL)==BUTTON_NONE);
 assert(livetv_remote_button(BUTTON_RC_UP)==LIVETV_BTN_UP);
 assert(livetv_remote_button(BUTTON_RC_LEFT)==BUTTON_NONE);
 assert(livetv_remote_button(BUTTON_RC_LEFT|BUTTON_REL)==LIVETV_BTN_LEFT);
 assert(livetv_remote_button(BUTTON_RC_SELECT)==BUTTON_NONE);
 assert(livetv_remote_button(BUTTON_RC_SELECT|BUTTON_REL)==LIVETV_BTN_SELECT);
 puts("PASS: production YouTube/Twitch adapter with core TV policy; four directions, Select/Play, Back/Home, held release, local controls, inactive TV/Kokkia; DIRECTV volume-coded arrows and repeat");
}
'''
code=base['code'].replace('int main(void)\n{',extra+'\nint main(void)\n{',1).replace('active=true;global_settings.tv_interface=1;','active=true;global_settings.tv_interface=1;check_app_remote();',1)
with tempfile.TemporaryDirectory(prefix='tv-app-remote-') as temp:
 p=Path(temp);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-g','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
