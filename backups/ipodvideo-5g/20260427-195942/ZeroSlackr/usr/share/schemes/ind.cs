\name Industrial
# By eddie19913.
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

  header: bg => @ind(titlebar).png, fg => white, line => #000000, accent => #6ae,
          shadow => #0039b3, shine => #87d0ff,
          gradient.top => graptop,
          gradient.middle => grapmid,
          gradient.bottom => grapbot,
          gradient.bar => grapbar +1
   music: bar => @ind(musfill).png ,
          bar.bg => @ind(musbar).png
 battery: border => #000000,
          bg => @ind(batbg).png,
          fill.normal => @ind(batfull).png,
          fill.low => @ind(batfull).png,
          fill.charge => @ind(batfull).png,
          bg.low =>   @ind(batbg).png,,
          bg.charging =>  @ind(batbg).png,
    lock: border => #282C28, fill => #383C40
 loadavg: bg => #E8F4E8, fg => #68D028, spike => #C0D0D8

  window: bg => black, fg => white, border => #333333 
  dialog: bg => black, fg => white, line => #333333,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => aquadbtn, button.sel.fg => black, button.sel.border => aquadbtnbdr, button.sel.inner => aquadbtn +1
   error: bg => white, fg => black, line => #808888,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => aquadbtn, button.sel.fg => black, button.sel.border => aquadbtnbdr, button.sel.inner => aquadbtn +1
  scroll: box => #000000,
          bg => @ind(scrollbar).png,
          bar => @ind(scroller).png
   input: bg => white, fg => black, selbg => aquadbtn, selfg => black, border => aquawinbdr, cursor => #808080

    menu: bg => black, fg => white, choice => black, icon => nearblack,
          selbg => <vert #FFFF00 to #000000>,
          selfg => <vert #000000 to FFFF00>, selchoice => <vert #000000 to FFFF00>,
# WTF are these used for?
          icon0 => #3b79da, icon1 => #28503c, icon2 => #50a078, icon3 => #ffffff
  slider: border => black, bg => @ind(musbar).png , full => @ind(musfill).png
textarea: bg => #000000, fg => #ffffff

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
