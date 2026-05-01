\name Iron
#by iameatingjam
\def black  #999999
\def white  #666666
\def nearblack #333333

\def aquabtn     #cccccc
\def aquadbtn    #cccccc
\def aquabtnbdr  #cccccc
\def aquadbtnbdr #cccccc
\def aquawinbdr  #cccccc

\def grapbot    #666666
\def grapmid    #666666
\def graptop    #666666
\def grapbar    #666666

  header: bg => @2.png, fg => #000000, line => #808888, accent => #6ae,
          shadow => #0039b3, shine => #87d0ff,
          gradient.top => graptop,
          gradient.middle => grapmid,
          gradient.bottom => grapbot,
          gradient.bar => grapbar +1
   music: bar => @music.bar.png ,
          bar.bg => @music.bar.bg.png
 battery: bg => #ffcc00,666666
          bg.low => #333333,
          bg.charging => #ccfcc,
    lock: border => #282C28, fill => #383C40
 loadavg: bg => #E8F4E8, fg => #68D028, spike => #C0D0D8

  window: bg => white, fg => black, border => #000000 -1
  dialog: bg => white, fg => black, line => #000000,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => aquadbtn, button.sel.fg => black, button.sel.border => aquadbtnbdr, button.sel.inner => aquadbtn +1
   error: bg => white, fg => black, line => #000000,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => aquadbtn, button.sel.fg => black, button.sel.border => aquadbtnbdr, button.sel.inner => aquadbtn +1
  scroll: box => #cccccc -1,
          bg => #333333,
          bar => @scroll.bar.png
   input: bg => white, fg => black, selbg => aquadbtn, selfg => black, border => aquawinbdr, cursor => #000000

    menu: bg => white, fg => black, choice => nearblack, icon => @OSXbutton.png,
          selbg => #cccccc,
          selfg => white, selchoice => white,

          icon0 => @OSXbutton1.png, icon1 => @OSXbutton1.png, icon2 => @OSXbutton1.png, icon3 => @OSXbutton1.png, icon4 => @OSXbutton1.png,
  slider: border => black, bg => @music.bar.bg.png , full => @music.bar.png
textarea: bg => #ffffff, fg => nearblack

box:
	default.bg => white,
	default.fg => black,
	default.border => #cccccc,
	selected.bg => #cccccc,
	selected.fg => #cccccc,
	selected.border => #000000,
	special.bg => white,
	special.fg => black,
	special.border => #000000

button:
	default.bg => white,
	default.fg => black,
	default.border => 000000,
	selected.bg => #cccccc,
	selected.fg => #cccccc,
	selected.border => #cccccc
