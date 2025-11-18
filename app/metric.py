import re
from datetime import datetime
from typing import List, Dict
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

def parse_log_file(log_content: str) -> Dict:
    """Parse server log and calculate speeds"""
    
    lines = log_content.strip().split('\n')
    
    uploads = []
    downloads = []
    connections = []
    

    upload_start = {}
    download_start = {}
    connection_start = None
    
    for line in lines:
        if '[ERROR]' in line or '[WARN]' in line:
            continue
            

        timestamp_match = re.match(r'\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\]', line)
        if not timestamp_match:
            continue
        timestamp = datetime.strptime(timestamp_match.group(1), '%Y-%m-%d %H:%M:%S')
        

        if 'Client connected to server' in line or 'Client successfully connected' in line:
            connection_start = timestamp
        elif ('Authentication successful' in line or 'User admin authenticated' in line) and connection_start:
            connection_time = (timestamp - connection_start).total_seconds()
            connections.append({
                'timestamp': timestamp,
                'duration': connection_time
            })
            connection_start = None
        

        if 'client_upload: sent' in line:
            sent_match = re.search(r'client_upload: sent (\d+) bytes', line)
            if sent_match:
                size_bytes = int(sent_match.group(1))

                upload_start['pending'] = {'start': timestamp, 'size': size_bytes}
        
        upload_match = re.search(r'Uploaded (\S+) for \S+ \((\d+) bytes\)', line)
        if upload_match:
            filename = upload_match.group(1)
            size_bytes = int(upload_match.group(2))

            if 'pending' in upload_start:
                duration = (timestamp - upload_start['pending']['start']).total_seconds()
                if duration >= 0:
                    uploads.append({
                        'filename': filename,
                        'size_bytes': size_bytes,
                        'duration': max(duration, 0.001), 
                        'speed_mbps': (size_bytes * 8) / (max(duration, 0.001) * 1_000_000),
                        'timestamp': timestamp
                    })
                upload_start.pop('pending')
        

        download_match = re.search(r'Sent (\S+) to client \((\d+) bytes\)', line)
        if download_match:
            filename = download_match.group(1)
            size_bytes = int(download_match.group(2))
            download_start[filename] = {'start': timestamp, 'size': size_bytes}
        
  
        download_complete_match = re.search(r'File download complete: (\S+) \((\d+) bytes\)', line)
        if download_complete_match:
            filename = download_complete_match.group(1)
            if filename in download_start:
                duration = (timestamp - download_start[filename]['start']).total_seconds()
                if duration >= 0:
                    downloads.append({
                        'filename': filename,
                        'size_bytes': download_start[filename]['size'],
                        'duration': max(duration, 0.001),  
                        'speed_mbps': (download_start[filename]['size'] * 8) / (max(duration, 0.001) * 1_000_000),
                        'timestamp': timestamp
                    })
                download_start.pop(filename)
    
    return {
        'uploads': uploads,
        'downloads': downloads,
        'connections': connections
    }

def format_size(bytes_size: int) -> str:
    """Convert bytes to human-readable format"""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if bytes_size < 1024.0:
            return f"{bytes_size:.2f} {unit}"
        bytes_size /= 1024.0
    return f"{bytes_size:.2f} TB"

def create_graphs(data: Dict):
    """Create visualizations for upload/download speeds and connection times"""
    
    fig, axes = plt.subplots(3, 1, figsize=(12, 10))
    fig.suptitle('Server Performance Metrics', fontsize=16, fontweight='bold', y=0.995)
    

    ax1 = axes[0]
    if data['uploads']:
        timestamps = [datetime.strptime(str(u.get('timestamp', datetime.now())), '%Y-%m-%d %H:%M:%S') 
                     if isinstance(u.get('timestamp'), str) else u.get('timestamp', datetime.now()) 
                     for u in data['uploads']]
        speeds = [u['speed_mbps'] for u in data['uploads']]
        
        ax1.plot(timestamps, speeds, marker='o', linewidth=2, markersize=8, color='#2E86AB', label='Upload Speed')
        ax1.fill_between(timestamps, speeds, alpha=0.3, color='#2E86AB')
        ax1.set_ylabel('Speed (Mbps)', fontsize=11, fontweight='bold')
        ax1.set_title('Server Upload Speed Over Time', fontsize=12, fontweight='bold')
        ax1.grid(True, alpha=0.3, linestyle='--')
        ax1.legend()
        
 
        for i, (ts, speed) in enumerate(zip(timestamps, speeds)):
            ax1.annotate(f'{speed:.2f} Mbps', 
                        xy=(ts, speed), 
                        xytext=(0, -15), 
                        textcoords='offset points',
                        ha='center',
                        fontsize=9)
    else:
        ax1.text(0.5, 0.5, 'No upload data available', 
                ha='center', va='center', transform=ax1.transAxes, fontsize=12)
        ax1.set_ylabel('Speed (Mbps)', fontsize=11, fontweight='bold')
        ax1.set_title('Server Upload Speed Over Time', fontsize=12, fontweight='bold')
    
    ax2 = axes[1]
    if data['downloads']:
        timestamps = [d.get('timestamp', datetime.now()) for d in data['downloads']]
        speeds = [d['speed_mbps'] for d in data['downloads']]
        
        ax2.plot(timestamps, speeds, marker='s', linewidth=2, markersize=8, color='#A23B72', label='Download Speed')
        ax2.fill_between(timestamps, speeds, alpha=0.3, color='#A23B72')
        ax2.set_ylabel('Speed (Mbps)', fontsize=11, fontweight='bold')
        ax2.set_title('Server Download Speed Over Time', fontsize=12, fontweight='bold')
        ax2.grid(True, alpha=0.3, linestyle='--')
        ax2.legend()
        
        for i, (ts, speed) in enumerate(zip(timestamps, speeds)):
            ax2.annotate(f'{speed:.2f} Mbps', 
                        xy=(ts, speed), 
                        xytext=(0, -15),
                        textcoords='offset points',
                        ha='center',
                        fontsize=9)
    else:
        ax2.text(0.5, 0.5, 'No download data available', 
                ha='center', va='center', transform=ax2.transAxes, fontsize=12)
        ax2.set_ylabel('Speed (Mbps)', fontsize=11, fontweight='bold')
        ax2.set_title('Server Download Speed Over Time', fontsize=12, fontweight='bold')
    
    ax3 = axes[2]
    if data['connections']:
        timestamps = [c['timestamp'] for c in data['connections']]
        durations = [c['duration'] * 1000 for c in data['connections']]
        
        ax3.plot(timestamps, durations, marker='^', linewidth=2, markersize=8, color='#F18F01', label='Connection Time')
        ax3.fill_between(timestamps, durations, alpha=0.3, color='#F18F01')
        ax3.set_ylabel('Time (ms)', fontsize=11, fontweight='bold')
        ax3.set_xlabel('Time', fontsize=11, fontweight='bold')
        ax3.set_title('User Connection Time Over Time', fontsize=12, fontweight='bold')
        ax3.grid(True, alpha=0.3, linestyle='--')
        ax3.legend()
        
      
        for i, (ts, dur) in enumerate(zip(timestamps, durations)):
            ax3.annotate(f'{dur:.1f} ms', 
                        xy=(ts, dur), 
                        xytext=(0, -15), 
                        textcoords='offset points',
                        ha='center',
                        fontsize=9)
    else:
        ax3.text(0.5, 0.5, 'No connection data available', 
                ha='center', va='center', transform=ax3.transAxes, fontsize=12)
        ax3.set_ylabel('Time (ms)', fontsize=11, fontweight='bold')
        ax3.set_xlabel('Time', fontsize=11, fontweight='bold')
        ax3.set_title('User Connection Time Over Time', fontsize=12, fontweight='bold')
    
    for ax in axes:
        ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
        ax.tick_params(axis='x', rotation=45)
    
    plt.tight_layout()
    plt.savefig('server_performance_metrics.png', dpi=300, bbox_inches='tight')
    print("\n Graphs saved as 'server_performance_metrics.png'")
    plt.show()

def display_results(data: Dict):
    """Display analysis results"""
    
    print("=" * 70)
    print("SERVER LOG ANALYSIS - SPEED METRICS")
    print("=" * 70)
    
    print("\n UPLOAD STATISTICS:")
    print("-" * 70)
    if data['uploads']:
        for i, upload in enumerate(data['uploads'], 1):
            print(f"\nUpload #{i}: {upload['filename']}")
            print(f"  Size:     {format_size(upload['size_bytes'])}")
            print(f"  Duration: {upload['duration']:.2f} seconds")
            print(f"  Speed:    {upload['speed_mbps']:.2f} Mbps")
        
        avg_upload_speed = sum(u['speed_mbps'] for u in data['uploads']) / len(data['uploads'])
        print(f"\n  Average Upload Speed: {avg_upload_speed:.2f} Mbps")
    else:
        print("  No successful uploads found")
    
    print("\n\n DOWNLOAD STATISTICS:")
    print("-" * 70)
    if data['downloads']:
        for i, download in enumerate(data['downloads'], 1):
            print(f"\nDownload #{i}: {download['filename']}")
            print(f"  Size:     {format_size(download['size_bytes'])}")
            print(f"  Duration: {download['duration']:.2f} seconds")
            print(f"  Speed:    {download['speed_mbps']:.2f} Mbps")
        
        avg_download_speed = sum(d['speed_mbps'] for d in data['downloads']) / len(data['downloads'])
        print(f"\n  Average Download Speed: {avg_download_speed:.2f} Mbps")
    else:
        print("  No successful downloads found")
    
    print("\n\n🔌 CONNECTION STATISTICS:")
    print("-" * 70)
    if data['connections']:
        for i, conn in enumerate(data['connections'], 1):
            print(f"\nConnection #{i}:")
            print(f"  Time to authenticate: {conn['duration']:.3f} seconds")
        
        avg_conn_time = sum(c['duration'] for c in data['connections']) / len(data['connections'])
        print(f"\n  Average Connection Time: {avg_conn_time:.3f} seconds")
    else:
        print("  No successful connections found")
    
    print("\n" + "=" * 70)

if __name__ == "__main__":
    log_file = "server-2025-11-14.log"
    
    try:
        with open(log_file, 'r') as f:
            log_content = f.read()
        
  
        results = parse_log_file(log_content, debug=True)
        
        display_results(results)
  
        create_graphs(results)
        
    except FileNotFoundError:
        print(f"Error: Could not find '{log_file}'")
        print("\nYou can also use it directly with log content:")
        print("\nlog_content = '''[paste log here]'''")
        print("results = parse_log_file(log_content)")
        print("display_results(results)")
