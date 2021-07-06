#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

import sys, os
import time
import functools
from queue import Queue

from PyQt5 import QtGui, QtWidgets
from PyQt5.QtCore import (QThread, QObject, pyqtSignal)
from util import hook

from gui import Ui_MainWindow

class MainWindow(QtWidgets.QMainWindow, Ui_MainWindow):
    ''' All GUI-self-consistent logic is handled in here (like updating liked fields) '''
    def __init__(self, parent = None):
        # Init "MainWindow" from Qty Designer export
        super(MainWindow, self).__init__()
        self.UI = Ui_MainWindow()
        self.UI.setupUi(self)
        #self.setWindowIcon(QtGui.QIcon(f'{PATH}/assets/icon.ico'))

        # TODO Read in values from the kernel module

        self.Update()       # Update the UI state depending on the values

        # ########## Hook to UI element events in order to process an action (frontend - self-consistent within the MainWindow)
        H = lambda ui, trigger, action, value = None : hook(getattr(self.UI,ui), trigger, action, value) # Convenient helper to reduce code blowup below
        # <UI Element>          <UI event Trigger>      <Action to execute>                                                                                                         <UI method to read value>
        H("PreScaleX",          "valueChanged",         lambda v : self.UI.PreScaleY.setValue(v) if not self.UI.EnablePreScaleY.isChecked() else None,                              "value")
        H("EnablePreScaleY",    "stateChanged",         lambda en: [self.UI.PreScaleY.setEnabled(en), self.UI.PreScaleY.setValue(self.UI.PreScaleX.value()) if not en else None],   "isChecked")
        H("PostScaleX",         "valueChanged",         lambda v : self.UI.PostScaleY.setValue(v) if not self.UI.EnablePostScaleY.isChecked() else False,                           "value")
        H("EnablePostScaleY",   "stateChanged",         lambda en: [self.UI.PostScaleY.setEnabled(en), self.UI.PostScaleY.setValue(self.UI.PostScaleX.value()) if not en else None],"isChecked")
    
    def Update(self):
        # Enable/Disable UI Elements depending on their value
        if self.UI.PreScaleX.value() == self.UI.PreScaleY.value():
            self.UI.PreScaleY.setEnabled(False)
            self.UI.EnablePreScaleY.setChecked(False)
        if self.UI.PostScaleX.value() == self.UI.PostScaleY.value():
            self.UI.PostScaleY.setEnabled(False)
            self.UI.EnablePostScaleY.setChecked(False)

class Leetmouse(QThread):
    ''' Main program: Merges UI and code and runs core program '''
    
    SYSFS_PARAMS = "/sys/bus/usb/drivers/leetmouse/module/parameters"

    def __init__(self):
        super().__init__()

        # UI init
        self.App = QtWidgets.QApplication(sys.argv)
        self.Window = MainWindow()
        self.UI = self.Window.UI
        self.Window.show()

        # Hook UI to events
        self.__hook()

        # Run program-code in separate thread (start() will call self.run, where all relevant background code will be handled via an envent-queue)
        self.queue = Queue()
        self.Running = True
        self.start()

        # Register exit handler
        self.App.aboutToQuit.connect(self.__onExit)
        sys.exit(self.App.exec_()) # Opens the MainWindow
    
    def enqueue(delay=0):
        """ As 'enqueued' decorated functions will add their action to the event-queue for queued (and probably) delayed execution """
        def decorator_enqueue(func):
            @functools.wraps(func)
            def wrapper(self, *args, **kwargs):
                def queued_item():
                    if delay != 0:
                        time.sleep(delay)
                    return func(self, *args, **kwargs)
                self.queue.put(queued_item)
        
            return wrapper
        
        return decorator_enqueue
    
    # Delay of one second is enforced in the driver itself! (Best offerts towards Anti-Cheats). So we already delay it here!
    @enqueue(delay=1)
    def applyParams(self):
        """ Apply new acceleration parameters to the leetmouse kernel module """
        self.readParams()
        
    def readParams(self):
        """ Reads the latest parameters from the kernel module """
        #### TODO write a dedicated/util class for reading & updating parameters to keep the main-program lean & mean
        with open(f"{self.SYSFS_PARAMS}/PreScaleX", 'r') as f:
            print(f.readline())

    def run(self):
        ''' Runs the continous calculation in separate thread via QThread, in order to not block the GUI thread '''
        while(self.Running):
            if not self.queue.empty():
                try:
                    self.queue.get()()
                except Exception as e:
                    print(f"EventQueue: {e}")
            time.sleep(0.05)
    
    def __onExit(self):
        ''' Exit handler '''
        self.Running = False    # Stops the QThread when exit signal has been triggered
    
    def __hook(self):
        ''' Hook to UI element events in order to process an action on the secondary thread '''
        # Define some convenient helpers to reduce code blowup below
        def H(ui, trigger, callback, value = None, update = None):
            if(update):
                def cb(v = None):
                    if(value == None):
                        callback()
                    else:
                        callback(v)
                    #self.__updateUI()
                hook(getattr(self.UI,ui), trigger, cb, value)
            else:
                hook(getattr(self.UI,ui), trigger, callback, value)
        
        # <UI Element>          <UI event Trigger>      <Callback to execute>   <UI method to read value>   <Instant-Update plots>
        H("Apply",              "clicked",              self.applyParams)
            
if __name__ == '__main__':
    Leetmouse()