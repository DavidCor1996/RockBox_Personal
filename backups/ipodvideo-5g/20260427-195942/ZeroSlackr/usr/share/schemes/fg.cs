\name Family Guy
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

  header: bg => @fg(titlebar).png, fg => black, line => #808888, accent => #6ae,
          shadow => #0039b3, shine => #87d0ff,
          gradient.top => graptop,
          gradient.middle => grapmid,
          gradient.bottom => grapbot,
          gradient.bar => grapbar +1
   music: bar => @familiar-(music.bar).png ,
          bar.bg => @familiar-(music.bar.bg).png
 battery: border => #00A1DB,
          bg => #00A1DB,
          fill.normal => @fg(bat).png,
          fill.low => @fg(bat).png,
          fill.charge => @fg(bat).png,
          bg.low => #00A1DB,
          bg.charging => #00A1DB
    lock: border => #282C28, fill => #383C40
 loadavg: bg => #E8F4E8, fg => #68D028, spike => #C0D0D8

  window: bg => #008CBF, fg => black, border => #808888 -1
  dialog: bg => <horiz #70CAEB to #00A1DB @:1,5>, fg => black, line => #808888,
          title.fg => black,
          button.bg => <horiz #00A1DB to #70CAEB @:1,5>, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => <horiz #815461 to #E2AE3B @:1,5>, button.sel.fg => white, button.sel.border => aquadbtnbdr, button.sel.inner => aquadbtn +1
   error: bg => white, fg => black, line => #808888,
          title.fg => black,
          button.bg => <horiz #00A1DB to #70CAEB @:1,5>, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => <horiz #815461 to #E2AE3B @:1,5>, button.sel.fg => white, button.sel.border => aquadbtnbdr, button.sel.inner => aquadbtn +1
  scroll: box => #56585a -1,
          bg => <horiz #815461 to #E2AE3B @:1,5>,
          bar => <horiz #70CAEB to #00A1DB @:1,5>
   input: bg => white, fg => black, selbg => aquadbtn, selfg => black, border => aquawinbdr, cursor => #808080

    menu: bg => #008CBF, fg => black, choice => nearblack, icon => nearblack,
          selbg => <vert #815461 to #E2AE3B @1,3>,
          selfg => white, selchoice => white,
# WTF are these used for?
          icon0 => #3b79da, icon1 => #28503c, icon2 => #50a078, icon3 => #ffffff
  slider: border => aquabtn, full => <vert #70CAEB to #00A1DB @:1,5> , bg => <vert #815461 to #E2AE3B @:1,5>
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
