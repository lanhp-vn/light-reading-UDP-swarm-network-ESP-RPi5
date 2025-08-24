import gpiod  # Library for GPIO control
import time
import socket
import threading

# Constants for GPIO pins and communication protocol
BUTTON_PIN = 26  # GPIO pin for the button
WHITE_LED_PIN = 16  # GPIO pin for white LED
RGB_LED_PINS = [5, 6, 13]  # GPIO pins for RGB LEDs
RPi_startBit = "+++"  # Start delimiter for messages
RPi_endBit = "***"  # End delimiter for messages
localPort = 4210  # Port to listen for incoming UDP messages

# LED flashing parameters and formula setup
led_state = 0  # Initial LED state
previous_time = 0  # Time of the last LED flash
x1, y1 = 24, 2010 / 1000  # Starting point for linear mapping
x2, y2 = 1024, 10 / 1000  # End point for linear mapping
slope = (y2 - y1) / (x2 - x1)  # Calculate slope for linear interpolation
intercept = y1 - slope * x1  # Calculate intercept for linear interpolation

# Initialize GPIO chip and request lines for button and LEDs
chip = gpiod.Chip('gpiochip4')
button_line = chip.get_line(BUTTON_PIN)
button_line.request(consumer="Button", type=gpiod.LINE_REQ_DIR_IN)
white_led_line = chip.get_line(WHITE_LED_PIN)
white_led_line.request(consumer="White_LED", type=gpiod.LINE_REQ_DIR_OUT, default_vals=[0])
rgb_led_lines = [chip.get_line(pin) for pin in RGB_LED_PINS]

# Request each RGB LED line and set initial state to off
for line in rgb_led_lines:
    line.request(consumer="RGB_LED", type=gpiod.LINE_REQ_DIR_OUT, default_vals=[0])

# Dictionary to track each Swarm ID's assigned LED index
swarm_id_to_led_index = {}
next_available_led_index = 0  # Tracks the next available LED to assign

# Set up UDP socket for communication and enable broadcast mode
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', localPort))
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

print(f"Listening for incoming messages on port {localPort}...")

# Global variable to track button state to detect changes
PREV_BUTTON_STATE = 0
RESET_REQUEST = False  # Tracks if a reset request is active

def reset_system():
    """Function to handle system reset, clear logs, reset LEDs, and broadcast reset message."""
    global next_available_led_index, RESET_REQUEST
    RESET_REQUEST = True  # Prevents other actions during reset

    # Broadcast reset request message to all devices
    reset_message = f"{RPi_startBit}RESET_REQUESTED{RPi_endBit}"
    print(f"Button is pressed. Broadcast: {reset_message}")
    sock.sendto(reset_message.encode('utf-8'), ('<broadcast>', localPort))

    # Clear sensor readings log file
    open("sensor_readings.txt", "w").close()

    # Reset Swarm ID to LED index mapping and LED control
    global swarm_id_to_led_index, next_available_led_index
    swarm_id_to_led_index = {}
    next_available_led_index = 0

    # Turn off all RGB LEDs
    for line in rgb_led_lines:
        line.set_value(0)

    # Flash white LED for 3 seconds to indicate reset
    white_led_line.set_value(1)
    time.sleep(3)
    white_led_line.set_value(0)
    RESET_REQUEST = False

def listen_for_messages():
    """Function to listen for UDP messages, process sensor data, and control RGB LEDs."""
    global previous_time, led_state, next_available_led_index, RESET_REQUEST
    while True:
        if not RESET_REQUEST:  # Skip listening if reset is active
            message, address = sock.recvfrom(1024)
            message = message.decode('utf-8')

            # Check for message start and end delimiters
            if message.startswith(RPi_startBit) and message.endswith(RPi_endBit):
                data = message[len(RPi_startBit):-len(RPi_endBit)]
                
                # Skip processing if message is reset request confirmation
                if data == "RESET_REQUESTED":
                    continue
                
                # Extract Swarm ID and analog reading from message
                swarm_id, analog_reading = data.split(',')

                print(f"Received from Swarm ID {swarm_id}: Analog Reading = {analog_reading}")

                # Log the sensor data to a text file
                with open("sensor_readings.txt", "a") as file:
                    file.write(f"Swarm ID {swarm_id}: {analog_reading}\n")

                # Assign an LED to new Swarm ID
                if swarm_id not in swarm_id_to_led_index:
                    swarm_id_to_led_index[swarm_id] = next_available_led_index
                    next_available_led_index = (next_available_led_index + 1) % len(RGB_LED_PINS)

                # Flash assigned LED based on analog reading interval
                led_index = swarm_id_to_led_index[swarm_id]
                for i in range(len(swarm_id_to_led_index)):
                    if i != led_index:
                        rgb_led_lines[i].set_value(0)  # Turn off other LEDs

                # Calculate flash interval using linear interpolation
                led_interval = slope * int(analog_reading) + intercept
                current_time = time.time()
                if current_time - previous_time >= led_interval:
                    previous_time = current_time
                    led_state = 1 - led_state  # Toggle LED state
                    rgb_led_lines[led_index].set_value(led_state)

def monitor_button():
    """Function to monitor button state and trigger system reset on button press."""
    global PREV_BUTTON_STATE
    while True:
        button_state = button_line.get_value()
        if button_state == 1 and PREV_BUTTON_STATE == 0:  # Detect button press
            reset_system()  # Call reset if button is pressed
        PREV_BUTTON_STATE = button_state
        time.sleep(0.1)  # Small delay to prevent button bounce issues

# Main entry point to start button monitoring and message listening threads
if __name__ == "__main__":
    # Create separate threads for button monitoring and message reception
    button_thread = threading.Thread(target=monitor_button)
    receive_thread = threading.Thread(target=listen_for_messages)

    # Start the threads
    button_thread.start()
    receive_thread.start()

    # Keep the program running by joining the threads
    button_thread.join()
    receive_thread.join()

    # Close the socket when finished (unreachable due to infinite loops)
    sock.close()