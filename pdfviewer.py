#!/usr/bin/python3
"""
PDF Viewer

This PDF viewer is entirely based on libfitz. While in the context of loading libraries into Python, the (A)GPL gets a bit fuzzy, we're just not going to take any chances and similarly release this reader under the (A)GPL, which is why you had to get it from our package repository instead of it being available in the core distribution for the OS.

If you have this file, you should have the AGPL as well - see the Help > Contents menu in the app itself.
"""
import os
import math
import stat
import sys
import subprocess

import ctypes

import cairo

import yutani
import text_region
import toaru_fonts

from menu_bar import MenuBarWidget, MenuEntryAction, MenuEntrySubmenu, MenuEntryDivider, MenuWindow
from icon_cache import get_icon
from about_applet import AboutAppletWindow

from dialog import OpenFileDialog

import yutani_mainloop

version = "0.1.0"
app_name = "PDF Viewer"
_description = f"<b>{app_name} {version}</b>\nCopyright 2017 Kevin Lange\nCopyright 2006-2012 Artifex Software, Inc.\n\nPDF Viewer based on MuPDF.\n\n<color 0x0000FF>http://mupdf.com/</color>"

class PDFViewerWindow(yutani.Window):

    base_width = 640
    base_height = 480

    def __init__(self, decorator, path):
        super(PDFViewerWindow, self).__init__(self.base_width + decorator.width(), self.base_height + decorator.height(), title=app_name, icon="pdfviewer", doublebuffer=True)
        self.move(100,100)
        self.x = 100
        self.y = 100
        self.decorator = decorator

        self.fitz_lib = ctypes.CDLL('libtoaru-fitz.so')
        self.fitz_lib.init_fitz.restype = ctypes.c_void_p
        self.fitz_lib.load_document.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        self.fitz_lib.load_document.restype = ctypes.c_void_p
        self.fitz_lib.set_fit.argtypes = [ctypes.c_int]
        self.fitz_lib.draw_page.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
        self.fitz_lib.page_count.argtypes = [ctypes.c_void_p]
        self.fitz_lib.page_count.restype = ctypes.c_int

        self.fitz_ctx = self.fitz_lib.init_fitz()

        def open_file(action):
            OpenFileDialog(self.decorator,"Open PDF...",glob="*.pdf",callback=self.load_file,window=self)

        def exit_app(action):
            menus = [x for x in self.menus.values()]
            for x in menus:
                x.definitely_close()
            self.close()
            sys.exit(0)
        def about_window(action):
            AboutAppletWindow(self.decorator,f"About {app_name}","/usr/share/icons/external/pdfviewer.png",_description,"pdfviewer")
        def help_browser(action):
            subprocess.Popen(["help-browser.py","downloaded/mupdf.trt"])
        menus = [
            ("File", [
                MenuEntryAction("Open","open",open_file,None),
                MenuEntryDivider(),
                MenuEntryAction("Exit","exit",exit_app,None),
            ]),
            ("Help", [
                MenuEntryAction("Contents","help",help_browser,None),
                MenuEntryDivider(),
                MenuEntryAction(f"About {app_name}","star",about_window,None),
            ]),
        ]

        self.menubar = MenuBarWidget(self,menus)

        self.hover_widget = None
        self.down_button = None

        self.menus = {}
        self.hovered_menu = None

        self.buf = None
        self.load_file(path)
        self.hilighted = None

    def load_file(self, path):
        if not path or not os.path.exists(path):
            self.document = None
            self.redraw_buf(True)
            self.page = 1
            self.page_count = 1
        else:
            self.path = path
            self.document = self.fitz_lib.load_document(self.fitz_ctx, path.encode('utf-8'))
            self.page = 1
            self.page_count = self.fitz_lib.page_count(self.document)
            self.redraw_buf(True)

    def go_up(self, action):
        pass

    def redraw_buf(self,size_changed=False):
        if size_changed:
            if self.buf:
                self.buf.destroy()
            w = self.width - self.decorator.width()
            h = self.height - self.decorator.height()
            self.buf = yutani.GraphicsBuffer(w,h)
        yutani.Window.fill(self.buf,0xFF777777)
        if self.document:
            self.fitz_lib.draw_page(self.fitz_ctx, self.document, self.page, self.buf._gfx)
            self.set_title(f"{self.path} - {self.page}/{self.page_count} - {app_name}","pdfviewer")
        else:
            self.set_title(app_name,"pdfviewer")


    def draw(self):
        surface = self.get_cairo_surface()

        WIDTH, HEIGHT = self.width - self.decorator.width(), self.height - self.decorator.height()

        ctx = cairo.Context(surface)
        ctx.translate(self.decorator.left_width(), self.decorator.top_height())
        ctx.rectangle(0,0,WIDTH,HEIGHT)
        ctx.set_source_rgb(1,1,1)
        ctx.fill()

        ctx.save()
        ctx.translate(0,self.menubar.height)
        text = self.buf.get_cairo_surface()
        ctx.set_source_surface(text,0,0)
        ctx.paint()
        ctx.restore()

        self.menubar.draw(ctx,0,0,WIDTH)

        self.decorator.render(self)
        self.flip()

    def finish_resize(self, msg):
        """Accept a resize."""
        if msg.width < 120 or msg.height < 120:
            self.resize_offer(max(msg.width,120),max(msg.height,120))
            return
        self.resize_accept(msg.width, msg.height)
        self.reinit()
        self.redraw_buf(True)
        self.draw()
        self.resize_done()
        self.flip()

    def scroll(self, amount):
        self.page += amount
        if self.page < 1:
            self.page = 1
        if self.page > self.page_count:
            self.page = self.page_count
        self.redraw_buf(False)


    def mouse_event(self, msg):
        if d.handle_event(msg) == yutani.Decor.EVENT_CLOSE:
            window.close()
            sys.exit(0)
        x,y = msg.new_x - self.decorator.left_width(), msg.new_y - self.decorator.top_height()
        w,h = self.width - self.decorator.width(), self.height - self.decorator.height()

        if x >= 0 and x < w and y >= 0 and y < self.menubar.height:
            self.menubar.mouse_event(msg, x, y)
            return

        if msg.buttons & yutani.MouseButton.SCROLL_UP:
            self.scroll(-1)
            self.draw()
            return
        elif msg.buttons & yutani.MouseButton.SCROLL_DOWN:
            self.scroll(1)
            self.draw()
            return

    def keyboard_event(self, msg):
        if msg.event.action != yutani.KeyAction.ACTION_DOWN:
            return # Ignore anything that isn't a key down.
        if msg.event.key == b"q":
            self.close()
            sys.exit(0)

if __name__ == '__main__':
    yutani.Yutani()
    d = yutani.Decor()

    window = PDFViewerWindow(d,None if len(sys.argv) < 2 else sys.argv[1])
    window.draw()

    yutani_mainloop.mainloop()

