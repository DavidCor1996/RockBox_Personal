\name Impressions
# by jtXdriggers
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

  header: bg => @impressions(titlebar).png, fg => black, line => #808888, accent => #6ae,
          shadow => #000000, shine => #888888,
          gradient.top => graptop,
          gradient.middle => grapmid,
          gradient.bottom => grapbot,
          gradient.bar => grapbar +1
   music: bar => @familiar-(music.bar).png ,
          bar.bg => @familiar-(music.bar.bg).png
 battery: border => #6E6E6E,
          bg => <vert #C7C7C7 to #979797 to #979797 to #C7C7C7>,
          fill.normal => <vert #FFFFFF to #3A3A3A to #FFFFFF>,
          fill.low => <vert #3A3A3A to #FFFFFF to #3A3A3A>,
          fill.charge => <vert #3A3A3A to #FFFFFF to #3A3A3A>,
          bg.low =>   <vert #C7C7C7 to #979797 to #979797 to #C7C7C7>,
          bg.charging =>  <vert #C7C7C7 to #979797 to #979797 to #C7C7C7>,
    lock: border => #282C28, fill => #383C40
 loadavg: bg => #E8F4E8, fg => #68D028, spike => #C0D0D8

  window: bg => <horiz #000000 to #000000 to #C4C4C4>, fg => black, border => #808888 -1
  dialog: bg => white, fg => black, line => #808888,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => <vert #AEAEAE to #FFFFFF>, button.sel.fg => white, button.sel.border => <vert #AEAEAE to #FFFFFF>bdr, button.sel.inner => <vert #AEAEAE to #FFFFFF> +1
   error: bg => white, fg => black, line => #808888,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => <vert #AEAEAE to #FFFFFF>, button.sel.fg => white, button.sel.border => <vert #AEAEAE to #FFFFFF>bdr, button.sel.inner => <vert #AEAEAE to #FFFFFF> +1
  scroll: box => #000000,
          bg => <horiz #000000 to #555555 to #4F4A4B>,
          bar => <horiz #3A3A3A to #FFFFFF to #3A3A3A>
   input: bg => white, fg => black, selbg => <vert #AEAEAE to #FFFFFF>, selfg => white, border => aquawinbdr, cursor => #808080

    menu: bg => <horiz #000000 to #000000 to #C4C4C4>, fg => black, choice => nearblack, icon => nearblack,
          selbg => <vert #FFFFFF to #3A3A3A to #FFFFFF>,
          selfg => #000000, selchoice => <vert #000000 to #FFFF00>,
# WTF are these used for?
          icon0 => #3b79da, icon1 => #28503c, icon2 => #50a078, icon3 => #ffffff
  slider: border => black, bg => <vert #000000 to #555555 to #4F4A4B>, full => <vert #3A3A3A to #FFFFFF to #3A3A3A>
textarea: bg => <horiz #000000 to #C4C4C4>, fg => white

box:
	default.bg => <vert #c3d6ff to #b4caf6 to #9dbaf6>,
	default.fg => black,
	default.border => #7f95db,
	selected.bg => <vert #3f80de to #2f63d5 to #1e41cd>,
	selected.fg => black,
	selected.border => #16a,
	special.bg => <vert #d5d6d5 to #d1cfd1 to #c5c6c5>,
	special.fg => black,
	special.border => #939393

button:
	default.bg => ,
	default.fg => black,
	default.border => aquabtnbdr,
	selected.bg => <vert #AEAEAE to #FFFFFF>,
	selected.fg => black,
	selected.border => <vert #AEAEAE to #FFFFFF>bdr
