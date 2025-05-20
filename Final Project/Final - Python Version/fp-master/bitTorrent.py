#!/usr/bin/env python3
import argparse
import math
import functions
from urllib.parse import urlparse


# Add more methods as needed
def parse_args():
    parser = argparse.ArgumentParser(description="Generate a .torrent file with port number.")
    parser.add_argument("input_file", type=str, help="Input file to create .torrent for")
    parser.add_argument("-p", "--port", type=int, default=6881, help="Port number for torrent tracker (default: 6881)")
    return parser.parse_args()

def main():
    args = parse_args()
    input_file = args.input_file
    cl_port = args.port

    # Add your logic here to generate .torrent file using input_file and port

    peer_id = functions.make_url_encode(functions.create_peer_id())
    #print(len(peer_id))
    
    decoded_data = functions.decode_torrent_file(input_file)
    torrent_info = functions.TorrentInfo(decoded_data)
    #print(torrent_info.length)
    
    url = torrent_info.announce.decode("utf-8")
    parsed_url = urlparse(url)
    host = parsed_url.hostname
    port = parsed_url.port
    #path = '/'
    #print(torrent_info.length)
    #print(torrent_info.piece_length)
    #print(torrent_info.piece_hashes)
    
    #print(port)
    
    #print(len(functions.make_url_encode(torrent_info.info_hash)))
    peers = functions.http_get_request(host, port, torrent_info, peer_id, cl_port)
    
    print("FILE NAME:", torrent_info.name)
    # for i in range(0, torrent_info.num_pieces):
    #     functions.download_piece(torrent_info, i, torrent_info.name, peers, peer_id)
    
    functions.download_torrent(torrent_info, peers, peer_id)

if __name__ == "__main__":
    main()
