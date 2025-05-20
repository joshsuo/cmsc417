#! /usr/bin/env python3

from io import BytesIO
import random
import time
from urllib.parse import urlencode, quote
import bencodepy
import socket
import hashlib
import struct
import math
import bitstring
import threading
import select

class TorrentInfo:
    def __init__(self, decoded_data):
        self.decoded_data = decoded_data
        self.announce = decoded_data.get(b'announce', None)
        self.announce_list = decoded_data.get(b'announce-list', [])
        self.comment = decoded_data.get(b'comment', None)
        self.created_by = decoded_data.get(b'created by', None)
        self.creation_date = decoded_data.get(b'creation date', None)
        self.info = decoded_data.get(b'info', {})
        self.length = self.info.get(b'length', None)
        self.piece_length = self.info.get(b'piece length', None)
        self.pieces = self.info.get(b'pieces', None)
        self.piece_hashes = [self.pieces[i:i+20].hex() for i in range(0, len(self.pieces), 20)]
        self.info_hash = self.calculate_info_hash()  # Calculate this if needed
        self.num_pieces = math.ceil(self.length / self.piece_length)
        self.bitfield = [0] * self.num_pieces
        self.name = self.info.get(b'name', None)

    def calculate_info_hash(self):
        # Logic to calculate the info hash from self.info
        if self.info:
            encoded_info = bencodepy.encode(self.info)
            #sha1_hash = hashlib.sha1(encoded_info).digest()
            self.info_hash = hashlib.sha1(encoded_info).digest()
            #print(len(self.info_hash))
            return self.info_hash
        else:
            return None

def make_url_encode(id_bytes):
    encoded_str = quote(id_bytes)
    #print(encoded_str)
    #print(len(encoded_str))
    return encoded_str

def create_peer_id():
    random.seed(time.time())
    client_id = "FP"
    version_number = 1924

    # Allocate memory for the peer ID (including null terminator)
    peer_id = [""] * 21  # 20 characters + null terminator

    # Format the peer ID string
    peer_id[:2] = "-" + client_id
    peer_id[3:7] = "{:04d}".format(version_number)

    # Generate and append random numbers to complete the peer ID
    remaining_length = 15#20 - len("".join(peer_id))  # Length remaining for random numbers

    #print(peer_id)
    #print(len(peer_id))
    for i in range(remaining_length):
        peer_id[7 + i] = str(random.randint(0, 9))  # Add random digit (0-9)

    id = "".join(peer_id)
    return id[:20]

def decode_torrent_file(input_file):
    with open(input_file, 'rb') as f:
        torrent_data = bencodepy.decode(f.read())
    return torrent_data

def http_get_request(host, port, torrent_file:TorrentInfo, peer_id, cl_port):
    
    try:
        # Create a socket object
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        # Connect to the server
        s.connect((host, port))
        
        # Send HTTP GET request
        request = f"GET /announce?info_hash={make_url_encode(torrent_file.info_hash)}&peer_id={peer_id}&port={cl_port}&uploaded=&{0}downloaded={0}&left={torrent_file.length}&compact={0}&event=started HTTP/1.1\r\nHost: {host}\r\n\r\n"

        #print(request.encode())
        s.sendall(request.encode())

        # Receive response
        response = b""
        while True:
            data = s.recv(1024)
            if not data:
                break
            response += data
        
        # Close the connection
        s.close()

        header, body = response.split(b'\r\n\r\n', 1)

        decoded_body = bencodepy.decode(body)

        #Get the peers field
        peers_binary = decoded_body[b'peers']

        # Parse the peers field
        peers = []
        for i in range(0, len(peers_binary), 6):
            ip = ".".join(map(str, peers_binary[i:i+4]))
            port = struct.unpack('>H', peers_binary[i+4:i+6])[0]
            peers.append((ip, port))

       # print(peers)
       # print(decoded_body)
        return peers
    except socket.error as err:
        print(f"Error: {err}")
        return None
    

def generate_handshake(torrent_content:TorrentInfo, peer_id):
    info_hash = torrent_content.info_hash
    handshake = b"\x13BitTorrent protocol\x00\x00\x00\x00\x00\x00\x00\x00"
    handshake += info_hash
    #print(type(peer_id))
    #print(type(handshake))
    handshake += peer_id.encode()
    return handshake


def handshake(torrent_content, peer_ip, peer_port, peer_id):
    handshake = generate_handshake(torrent_content, peer_id)
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((peer_ip, int(peer_port)))
        s.send(handshake)
        response_handshake = s.recv(len(handshake))

    if(torrent_content.info_hash == response_handshake[28:48] and peer_id == response_handshake[48:68]):
        print("Handshake successful")
    else:
        print("Handshake failed")
        s.close()
        return None
    
    return response_handshake


def download_piece(torrent_current, piece_index, output_file, peers_list, peer_id, lock):
    peers_list.append(("0.0.0.0", -1))
    idx = 0
    upper_bound = 10

    while idx < len(peers_list):
        
        peer_ip, peer_port = peers_list[idx]
        idx += 1

        if(peer_ip == "0.0.0.0" and peer_port == -1):
            idx = 0

            if (upper_bound == 0):
                print(f"Piece unavailable: {piece_index}")
                return None
            
            upper_bound -= 1
            continue

        # open a connection to the peer
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(2)
            # print("Connecting to peer", peer_ip, peer_port)
            # Connect to current peer
            try:
                s.connect((peer_ip, int(peer_port)))            
            except socket.timeout:
                continue
            except Exception as e:
                continue
            # Initiate handshake with peer
            s.settimeout(5)
            # print("Connected to peer", peer_ip, peer_port)
            handshake = generate_handshake(torrent_current, peer_id)
            s.sendall(handshake)
            # receive the handshake response
            response_handshake = "".encode()
            try:
                response_handshake = s.recv(len(handshake))
            except socket.timeout:
                # print("Handshake No Go")
                s.close()
                continue
            except Exception as e:
                # print("THEY DONT WANT TO SHAKE HANDS")
                s.close()
                continue

            # verify the handshake response
            if(torrent_current.info_hash != response_handshake[28:48]):   
                s.close()
                continue

            # receive the bitfield message
            s.settimeout(5)
            try:
                length, msg_type = s.recv(4), s.recv(1)
            except socket.timeout:
                # print("Peer is broke")
                s.close()
                continue
            except Exception as e:
                # print("Peer closed sock")
                s.close()
                continue
            
            # if not bitfield message, then close
            if msg_type != b"\x05":
                s.close()
                continue
            # transform bitfield message to bit array
            
            try:
                bitfield = s.recv(int.from_bytes(length, byteorder="big") - 1)
            except Exception as e:
                # print("Peer closed sock")
                s.close()
                continue
            
            bit_array = bitstring.ConstBitStream(bytes=bitfield).bin
            # check if the peer has the piece we want, send interested message

            if bit_array[piece_index] == "1":
                try:
                    s.sendall(b"\x00\x00\x00\x01\x02")
                except Exception as e:
                    s.close()
                    continue
            else:
                try:
                    s.sendall(b"\x00\x00\x00\x01\x03")
                    s.close()
                    continue
                except Exception as e:
                    s.close()
                    continue

            # receive the unchoke message
            try:
                length, msg_type = s.recv(4), s.recv(1)
            except socket.timeout:
                # print("Choked")
                s.close()
                continue
            except Exception as e:
                # print("Peer closed sock")
                s.close()
                continue
            
            # if message is not unchoked, then close
            if msg_type != b"\x01":
                s.close()
                # print("They choked")
                continue
            
            piece_length = torrent_current.piece_length
            chuck_size = 16 * 1024 # piece size

            if piece_index == (len(torrent_current.pieces) // 20) - 1:
                piece_length = (
                    torrent_current.pieces % piece_length
                )

            piece = b""
            # picking the piece from work queue
            s.settimeout(15)
            
            try:
                for i in range(math.ceil(piece_length / chuck_size)):
                    
                    #print("ok")
                    msg_id = b"\x06" # request for a file to download

                    chunk_index = piece_index.to_bytes(4, byteorder='big')
                    chunk_begin = (i * chuck_size).to_bytes(4, byteorder='big')
                    if i == math.ceil(piece_length / chuck_size) - 1 and piece_length % chuck_size != 0:
                        chunk_length = (piece_length % chuck_size).to_bytes(4, byteorder='big')
                    else:
                        chunk_length = chuck_size.to_bytes(4, byteorder='big')
                    message_length = (1 + len(chunk_index) + len(chunk_begin) + len(chunk_length)).to_bytes(4, byteorder='big')
                    request_message = message_length + msg_id + chunk_index + chunk_begin + chunk_length
                    s.sendall(request_message)

                    # print(f"Requesting piece: {int.from_bytes(chunk_index, 'big')}, begin: {int.from_bytes(chunk_begin, 'big')}, length: {int.from_bytes(chunk_length, 'big')}")
                    msg = msg_id + chunk_index + chunk_begin + chunk_length
                    msg = len(msg).to_bytes(4, byteorder='big') + msg
                    length, msg_type = int.from_bytes(s.recv(4), byteorder='big'), s.recv(1)
                    resp_index = int.from_bytes(s.recv(4), byteorder='big')
                    resp_begin = int.from_bytes(s.recv(4), byteorder='big')
                    block = b""
                    to_get = int.from_bytes(chunk_length, byteorder='big')
                    while len(block) < to_get:
                        block += s.recv(to_get - len(block))
                    piece += block
            except Exception as e:
                # print("Peer closed sock")
                s.close()
                continue

            #check the integrity
            og_hash = torrent_current.pieces[piece_index * 20 : piece_index * 20 + 20]
            piece_hash = hashlib.sha1(piece).digest()
            # print(piece_hash)
            # print(og_hash)
            if piece_hash != og_hash:
                s.close()
                continue

            # writing the downloaded piece to a file
            with lock:
                with open(output_file, "r+b") as f:
                    f.seek(piece_index * torrent_current.piece_length)
                    f.write(piece)
            s.close()
            print(f"Finished Writing at index {piece_index} of the file.")

            # printProgressBar(iteration=piece_index, total=piece_length)
            return None



def download_torrent(torrent_info, peers, peer_id):
    file_name = torrent_info.name
    lock = threading.Lock()
    threads = []

    # Create an empty file with the correct size
    with open(file_name, 'wb') as f:
        f.truncate(torrent_info.length)

    for i in range(torrent_info.num_pieces):
        thread = threading.Thread(target=download_piece, args=(torrent_info, i, file_name, peers, peer_id, lock))
        threads.append(thread)
        thread.start()

    for thread in threads:
        thread.join()