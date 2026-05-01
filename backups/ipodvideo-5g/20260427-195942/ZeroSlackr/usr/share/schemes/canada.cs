\name Canada	
# by terrablebyte
\def white  #000000
\def black  #ffffff
\def nearblack #eeeeee

\def aquabtn     #000000
\def aquabtnbdr  #ffffff
\def <vert #AEAEAE to #FFFFFF>bdr #000000
\def aquawinbdr  #ffffff

\def grapbot    #000000
\def grapmid    #333333
\def graptop    #555555
\def grapbar    #888888

  header: bg => @canada(titlebar).png, fg => black, line => #808888, accent => #6ae,
          shadow => #000000, shine => #888888,
          gradient.top => graptop,
          gradient.middle => grapmid,
          gradient.bottom => grapbot,
          gradient.bar => grapbar +1
   music: bar => #010708 ,
          bar.bg => #979797
 battery: border => #6E6E6E,
          bg => <vert #C7C7C7 to #979797 to #979797 to #C7C7C7>,
          fill.normal => <vert #cc2626 to #e2626 to #cc2626>,
          fill.low => <vert #cc2626 to #e2626 to #cc2626>,
          fill.charge => <vert #cc2626 to #e2626 to #cc2626>,
          bg.low =>   <vert #C7C7C7 to #979797 to #979797 to #C7C7C7>,
          bg.charging =>  <vert #C7C7C7 to #979797 to #979797 to #C7C7C7>,
    lock: border => #282C28, fill => #383C40
 loadavg: bg => #E8F4E8, fg => #68D028, spike => #C0D0D8

  window: bg => #fe0000, fg => black, border => #808888 -1
  dialog: bg => white, fg => black, line => #808888,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => <vert #AEAEAE to #FFFFFF>, button.sel.fg => black, button.sel.border => <vert #AEAEAE to #FFFFFF>bdr, button.sel.inner => <vert #AEAEAE to #FFFFFF> +1
   error: bg => white, fg => black, line => #808888,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => <vert #AEAEAE to #FFFFFF>, button.sel.fg => black, button.sel.border => <vert #AEAEAE to #FFFFFF>bdr, button.sel.inner => <vert #AEAEAE to #FFFFFF> +1
  scroll: box => #888888,
          bg => <#ffffff>,
          bar => <horiz #cc2626 to #e2626 to #cc2626>
   input: bg => white, fg => black, selbg => <vert #AEAEAE to #FFFFFF>, selfg => black, border => aquawinbdr, cursor => #808080

    menu: bg => #fe0000
          selbg => <vert #cc2626 to #e2626 to #cc2626>,
          selfg => #9ED7FF, selchoice => <vert #000000 to #FFFF00>,
# WTF are these used for?
          icon0 => #3b79da, icon1 => #28503c, icon2 => #50a078, icon3 => #ffffff
  slider: border => black, bg => <vert #C7C7C7 to #979797 to #979797 to #C7C7C7>, full => <vert #208dd9 to #6EC3FF to #0A77C3>
textarea: bg => #ffffff, fg => nearblack

box:
	default.bg => <vert #c3d6ff to #b4caf6 to #9dbaf6>,
	default.fg => black,
	default.border => #7f95db,
	selected.bg => <vert #3f80de to #2f63d5 to #1e41cd>,
	selected.fg => white,
	selected.border => #16a,
	special.bg => <vert #d5d6d5 to #d1cfd1 to #c5c6c5>,
	special.fg => black,
	special.border => #939393

button:
	default.bg => aquabtn,
	default.fg => black,
	default.border => aquabtnbdr,
	selected.bg => <vert #AEAEAE to #FFFFFF>,
	selected.fg => black,
	selected.border => <vert #AEAEAE to #FFFFFF>bdr
