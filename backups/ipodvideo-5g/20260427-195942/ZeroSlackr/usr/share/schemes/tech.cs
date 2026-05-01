\name BlueTech
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

  header: bg => @tech(titlebar).png, fg => #9ED7FF, line => #1a1a1a, accent => #6ae,
          shadow => #0039b3, shine => #87d0ff,
          gradient.top => graptop,
          gradient.middle => grapmid,
          gradient.bottom => grapbot,
          gradient.bar => grapbar +1
   music: bar => <vert #208dd9 to #0A77C3> ,
          bar.bg => <vert #C7C7C7 to #979797 to #979797 to #C7C7C7>
 battery: border => #6E6E6E,
          bg => <vert #C7C7C7 to #979797 to #979797 to #C7C7C7>,
          fill.normal => <vert #208dd9 to #6EC3FF to #0A77C3>,
          fill.low => <vert #208dd9 to #6EC3FF to #0A77C3>,
          fill.charge => <vert #208dd9 to #6EC3FF to #0A77C3>,
          bg.low =>   <vert #C7C7C7 to #979797 to #979797 to #C7C7C7>,
          bg.charging =>  <vert #C7C7C7 to #979797 to #979797 to #C7C7C7>,
    lock: border => #282C28, fill => #383C40
 loadavg: bg => #E8F4E8, fg => #68D028, spike => #C0D0D8

  window: bg => #C7C7C7, fg => #6E6E6E, border => #333333 
  dialog: bg => #C7C7C7, fg => #6E6E6E, line => #333333,
          title.fg => #6E6E6E,
          button.bg => <vert #C7C7C7 to #979797 to #979797 to #C7C7C7>, button.fg => #6e6e6e, button.border => #6e6e6e,
          button.sel.bg => <vert #208dd9 to #0A77C3>, button.sel.fg => #9ED7FF, button.sel.border => #6e6e6e, button.sel.inner => #979797
   error: bg => , fg => black, line => #808888,
          title.fg => black,
          button.bg => aquabtn, button.fg => black, button.border => aquabtnbdr,
          button.sel.bg => aquadbtn, button.sel.fg => black, button.sel.border => aquadbtnbdr, button.sel.inner => aquadbtn +1
  scroll: box => #000000,
          bg => <horiz #C7C7C7 to #979797 to #979797 to #C7C7C7>,
          bar => <horiz #208dd9 to #6EC3FF to #0A77C3>
   input: bg => white, fg => black, selbg => aquadbtn, selfg => black, border => aquawinbdr, cursor => #808080

    menu: bg => #C7C7C7, fg => #6E6E6E, choice => black, icon => nearblack,
          selbg => <vert #208dd9 to #6EC3FF to #0A77C3>,
          selfg => #9ED7FF, selchoice => <vert #000000 to #FFFF00>,
# WTF are these used for?
          icon0 => #3b79da, icon1 => #28503c, icon2 => #50a078, icon3 => #ffffff
  slider: border => black, bg => <vert #C7C7C7 to #979797 to #979797 to #C7C7C7>, full => <vert #208dd9 to #6EC3FF to #0A77C3>
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
