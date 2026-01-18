ma_result AudioPlayer::ds_read(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead) {
    AudioPlayer* player = (AudioPlayer*)pDecoder->pUserData;
    size_t bytesRead = 0;
    uint8_t* outPtr = (uint8_t*)pBufferOut;
    
    while (bytesToRead > 0) {
        std::unique_lock<std::mutex> lock(player->m_bufferMutex);
        
        // Wait for data
        player->m_bufferCV.wait_for(lock, std::chrono::milliseconds(100), [player]() {
            return !player->m_rollingBuffer.empty() || player->m_stopSignal || player->m_seekRequested;
        });
        
        if (player->m_stopSignal || player->m_seekRequested) break;
        
        if (player->m_rollingBuffer.empty()) {
            // Buffer underrun or EOF
            break;
        }
        
        // Read from front chunk
        AudioChunk& chunk = player->m_rollingBuffer.front();
        size_t available = chunk.data.size() - player->m_readOffsetInFrontChunk;
        size_t toCopy = (bytesToRead < available) ? bytesToRead : available;
        
        if (toCopy > 0) {
            std::memcpy(outPtr, chunk.data.data() + player->m_readOffsetInFrontChunk, toCopy);
            
            outPtr += toCopy;
            bytesRead += toCopy;
            bytesToRead -= toCopy;
            player->m_readOffsetInFrontChunk += toCopy;
        }
        
        // Remove chunk if fully consumed
        if (player->m_readOffsetInFrontChunk >= chunk.data.size()) {
            player->m_readOffsetInFrontChunk = 0;
            player->m_rollingBuffer.pop_front();
            player->m_bufferCV.notify_all(); // Notify producer that space is available
        }
    }
    
    if (pBytesRead) *pBytesRead = bytesRead;
    return MA_SUCCESS; // Keep returning success even on partial read (EOF handled by bytesRead)
}

ma_result AudioPlayer::ds_seek(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin) {
    return MA_SUCCESS; // TODO: Implement seeking
}
