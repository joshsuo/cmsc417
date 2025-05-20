import hashlib
import bencodepy

def calculate_info_hash(torrent_path):
    with open(torrent_path, 'rb') as file:
        torrent_data = bencodepy.decode(file.read())
    with open("output.torrent", 'rb') as file:
        data = file.read()

    info_data = torrent_data[b'info']
    print(info_data)
    print()
    print(data)
    
    

    info_bencoded = bencodepy.encode(info_data)
    # data_bencoded = bencodepy.encode(data)

    print()
    print("encoding")
    print(info_bencoded)
    print()
    print(data)


    info_hash = hashlib.sha1(info_bencoded).hexdigest()
    data_hash = hashlib.sha1(data).hexdigest()
    print()
    print("HASHES")
    print(info_hash)
    print(data_hash)
    return info_hash

calculate_info_hash('sample.torrent')
