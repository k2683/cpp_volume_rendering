import os
import subprocess
import pandas as pd
from tqdm import tqdm

# 设置目录路径
dir1 = r"E:\cpp_volume_rendering\data\iso without skipping\img"
dir2 = r"E:\cpp_volume_rendering\data\iso with skipping\img"
output_excel = "ssim_comparison_results.xlsx"

# 存储结果的列表
results = []

# 遍历0000-0199的图片
for i in tqdm(range(200)):
    img_name = f"{i:04d}.png"
    img1_path = os.path.join(dir1, img_name)
    img2_path = os.path.join(dir2, img_name)
    
    # 确保两个文件都存在
    if os.path.exists(img1_path) and os.path.exists(img2_path):
        # 运行ImageMagick的compare命令
        cmd = f'magick compare -metric SSIM "{img1_path}" "{img2_path}" NULL:'
        try:
            # 执行命令并捕获输出
            process = subprocess.Popen(cmd, 
                                    stdout=subprocess.PIPE, 
                                    stderr=subprocess.PIPE,
                                    shell=True)
            _, stderr = process.communicate()
            
            # SSIM值会在stderr中
            ssim = float(stderr.decode().strip())
            
            # 添加结果
            results.append({
                'Image': img_name,
                'SSIM': ssim
            })
            
        except Exception as e:
            print(f"处理图片 {img_name} 时出错: {str(e)}")
            results.append({
                'Image': img_name,
                'SSIM': 'Error'
            })
    else:
        print(f"找不到图片: {img_name}")
        results.append({
            'Image': img_name,
            'SSIM': 'Missing'
        })

# 创建DataFrame并保存到Excel
df = pd.DataFrame(results)
df.to_excel(output_excel, index=False)
print(f"\n结果已保存到 {output_excel}")

# 计算并显示统计信息
valid_ssim = df[df['SSIM'] != 'Error'][df['SSIM'] != 'Missing']['SSIM']
if not valid_ssim.empty:
    print("\n统计信息:")
    print(f"平均SSIM: {valid_ssim.mean():.4f}")
    print(f"最小SSIM: {valid_ssim.min():.4f}")
    print(f"最大SSIM: {valid_ssim.max():.4f}")