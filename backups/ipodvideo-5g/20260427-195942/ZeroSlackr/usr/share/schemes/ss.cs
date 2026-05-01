\name Sleak Silver
# by eddie19913
\def black  #000000
\def white  #ffffff
\def nearblack #111111

\def aquabtn     #ececec
\def aquadbtn    #6cabed
\def aquabtnbdr  #5f5f5f
\def aquadbtnbdr #272799
\def aquawinbdr  #b8b8b8

\def grapbot    #D0D8D8
\def grapmid    #F0F4F8
\def graptop    #F0F4F8
\def grapbar    #F0F4F8

  header: bg => @ss(titlebar).png, fg => black, line => #808888, accent => #6ae,
          shadow => #0039b3, shine => #87d0ff,
          gradient.top => graptop,
          gradient.middle => grapmid,
          gradient.bottom => grapbot,
          gradient.bar => grapbar +1
   music: bar => @ss(music.bar).png ,
          bar.bg => @ss(music.bar.bg).png
 battery: border => #7f7f7f,
          bg => <vert #BEBEBE to #DADADA>,
          fill.normal => <vert #9A9A9A to #D5d5d5>,
          fill.low => #C03020,
          fill.charge => <vert #9A9A9A to #D5d5d5>,
          bg.low =>  <vert #BEBEBE to #DADADA>,
          bg.charging => <vert #BEBEBE to #DADADA>
    lock: border => #282C28, fill => #383C40
 loadavg: bg => #E8F4E8, fg => #68D028, spike => #C0D0D8

  window: bg => #E2E2E2, fg => black, border => #808888 -1
  dialog: bg => #E2E2E2, fg => black, line => #808888,
          title.fg => black,
          button.bg => white, button.fg => black, button.border => white,
          button.sel.bg => <vert #9A9A9A to #D5d5d5>, button.sel.fg => black, button.sel.border => white, button.sel.inner => white -1
   error: bg => #E2E2E2, fg => black, line => #808888,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => aquadbtn, button.sel.fg => black, button.sel.border => aquadbtnbdr, button.sel.inner => aquadbtn +1
  scroll: box => #c0c8c8,
          bg => <horiz #c0c8c8 to #d8e2e6 to #e8f2f3>,
          bar => @ss(scroll.bar).png
   input: bg => white, fg => black, selbg => <vert #9A9A9A to #D5d5d5>, selfg => black, border => white, cursor => #808080

    menu: bg => #E2E2E2, fg => black, choice => nearblack, icon => nearblack,
          selbg => <vert #D5D5D5 to #9A9a9a>,
          selfg => white, selchoice => white,
# WTF are these used for?
          icon0 => #3b79da, icon1 => #28503c, icon2 => #50a078, icon3 => #ffffff
  slider: border => black, bg => @ss(music.bar.bg).png , full => @ss(music.bar).png
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
	selected.bg => aquadbtn,
	selected.fg => black,
	selected.border => aquadbtnbdr
