#!/usr/bin/env python3

from bibliopixel.drivers.channel_order import ChannelOrder
from bibliopixel.drivers.spi_interfaces import SPI_INTERFACES
from bibliopixel.drivers.SPI.LPD8806 import LPD8806
from bibliopixel.layout import Strip
import paho.mqtt.subscribe as subscribe
import re
import sys
import time


class Door(object):
    VACANT = 1
    OCCUPIED = 0
    VACANT_COLOR = (0,255,0)
    OCCUPIED_COLOR = (255,0,0)
    INVALID_COLOR = (0,0,255)

    def __init__(self, sensor, state, start_addr, led_count):
        self.sensor_name = sensor
        self.state = state
        self.start_addr = start_addr
        self.led_count = led_count

    def getState(self, state):
        return self.state

    def setState(self, state):
        self.state = state

    def setLEDs(self, led_buffer):
        color = Door.INVALID_COLOR
        if self.state == Door.VACANT:
            color = Door.VACANT_COLOR
        if self.state == Door.OCCUPIED:
            color = Door.OCCUPIED_COLOR
        print("Using color {}".format(color))
        for i in range(self.led_count):
            led_buffer.set(self.start_addr + i, color)


class VacancySign(object):

    def __init__(self, host, door_count, leds_per_door):
        self.host = host
        self.doors = {}
        self.door_regex = re.compile(r"door/(sensor[0-9A-F]+)/status")
        pixel_count = door_count * leds_per_door
        self.led_driver = LPD8806(num=pixel_count, dev="/dev/spidev0.0", c_order=ChannelOrder.BRG, spi_interface=SPI_INTERFACES.PERIPHERY, spi_speed=2)
        self.led_driver.set_brightness(128)
        self.leds = Strip(self.led_driver)
        self.next_addr = 0
        self.addr_stride = leds_per_door
        self.clear()

    def onMessage(self, client, userdata, message):
        print("{} {}".format(message.topic, message.payload))
        matches = self.door_regex.match(message.topic)
        if matches:
            sensor = matches.group(1)
            state = int(message.payload)
            if sensor in self.doors.keys():
                self.doors[sensor].setState(state)
            else:
                self.doors[sensor] = Door(matches.group(1), state, self.next_addr, self.addr_stride)
                self.next_addr += self.addr_stride
            self.updateDisplay()

    def updateDisplay(self):
        self.leds.all_off()
        for door in self.doors.values():
            door.setLEDs(self.leds)
        self.leds.update()

    def run(self):
        subscribe.callback(self.onMessage, "door/+/status", hostname=self.host)

    def clear(self):
        self.leds.all_off()
        self.leds.update()


def main():
    sign = VacancySign("localhost", 3, 2)
    try:
        while True:
            sign.run()
    except KeyboardInterrupt:
        sign.clear()
        sys.exit(0)

main()
