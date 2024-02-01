import chromadb
chroma_client = chromadb.HttpClient(host="localhost", port=8000)
print('HEARTBEAT:', chroma_client.heartbeat())