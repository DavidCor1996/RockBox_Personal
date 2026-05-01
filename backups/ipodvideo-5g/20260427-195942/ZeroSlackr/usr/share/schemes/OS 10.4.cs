\name OS 10.4
#by hatchinatore
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

  header: bg => @mtitle.png, fg => black, line => #ffffff, accent => #6ae,
          shadow => #0039b3, shine => #87d0ff,
          gradient.top => graptop,
          gradient.middle => grapmid,
          gradient.bottom => grapbot,
          gradient.bar => grapbar +1
   music: bar => @ml.png ,
          bar.bg => @mbbg.png
 battery: bg => @mbb.png,
          fill.normal => @mb.png,
          fill.low => @mb.png,
          fill.charge => @mbc.png,
          bg.low =>  @mbb.png,
          bg.charging => @mbc.png
    lock: border => #000000, fill => #0000ff
 loadavg: bg => #E8F4E8, fg => #68D028, spike => #C0D0D8

  window: bg => @mbg.png, fg => @mpopup.png, border => #808888 -1
  dialog: bg => @mpopup.png, fg => @mpopup.png, line => #808888,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => aquadbtn, button.sel.fg => black, button.sel.border => aquadbtnbdr, button.sel.inner => aquadbtn +1
   error: bg => @mpopup.png, fg => black, line => #808888,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => aquadbtn, button.sel.fg => black, button.sel.border => aquadbtnbdr, button.sel.inner => aquadbtn +1
  scroll: box => #56585a -1,
          bg => @msbg.png,
          bar => @ms.png
   input: bg => white, fg => black, selbg => aquadbtn, selfg => black, border => aquawinbdr, cursor => #808080

    menu: bg => #19489B, fg => #000000, choice => nearblack, icon => @OSXbutton.png,
          selbg => <vert #209bd6 to #0073bc>,
          selfg => white, selchoice => white,
# WTF are these used for?
          icon0 => @OSXbutton1.png, icon1 => @OSXbutton1.png, icon2 => @OSXbutton1.png, icon3 => @OSXbutton1.png, icon4 => @OSXbutton1.png,
  slider: border => black, bg => @mbbg.png , full => @ml.png
textarea: bg => #29549D, fg => nearblack

box:
	default.bg => <vert #c3d6ff to #b4caf6 to #9dbaf6>,
	default.fg => black,
	default.border => #7f95db,
	selected.bg => <vert #4f90ee to #2f63d5 to #1e41cd>,
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
