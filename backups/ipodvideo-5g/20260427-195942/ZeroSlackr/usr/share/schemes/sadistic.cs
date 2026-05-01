\name Sadistic
# pz2s evil twin.
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

  header: bg => @sadistic(titlebar).png, fg => black, line => #808888, accent => #6ae,
          shadow => #000000, shine => #888888,
          gradient.top => graptop,
          gradient.middle => grapmid,
          gradient.bottom => grapbot,
          gradient.bar => grapbar +1
   music: bar => @familiar-(music.bar).png ,
          bar.bg => @familiar-(music.bar.bg).png
 battery: border => #000000,
          bg => <vert #a0adb8 to #d8e2e6>,
          fill.normal => @sadistic(batfull).png,
          fill.low => #C03020,
          fill.charge => @sadistic(battfull).png,
          bg.low =>  <vert #a0adb8 to #d8e2e6>,
          bg.charging => <vert #a0adb8 to #d8e2e6>
    lock: border => #282C28, fill => #383C40
 loadavg: bg => #E8F4E8, fg => #68D028, spike => #C0D0D8

  window: bg => white, fg => black, border => #808888 -1
  dialog: bg => white, fg => black, line => #808888,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => <vert #AEAEAE to #FFFFFF>, button.sel.fg => black, button.sel.border => <vert #AEAEAE to #FFFFFF>bdr, button.sel.inner => <vert #AEAEAE to #FFFFFF> +1
   error: bg => white, fg => black, line => #808888,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => <vert #AEAEAE to #FFFFFF>, button.sel.fg => black, button.sel.border => <vert #AEAEAE to #FFFFFF>bdr, button.sel.inner => <vert #AEAEAE to #FFFFFF> +1
  scroll: box => #888888,
          bg => <horiz #000000 to #555555 to #4F4A4B>,
          bar => @sadistic(scroll.bar).png
   input: bg => white, fg => black, selbg => <vert #AEAEAE to #FFFFFF>, selfg => black, border => aquawinbdr, cursor => #808080

    menu: bg => white, fg => black, choice => nearblack, icon => nearblack,
          selbg => <vert #AEAEAE to #FFFFFF>,
          selfg => white, selchoice => white,
# WTF are these used for?
          icon0 => #3b79da, icon1 => #28503c, icon2 => #50a078, icon3 => #ffffff
  slider: border => black, bg => @sadistic(music.bar.bg).png , full => @sadistic(music.bar).png
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
