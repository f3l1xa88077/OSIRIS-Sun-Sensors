import csv
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

# Function to convert spherical to Cartesian
def spherical_to_cartesian_custom(R, theta_deg, phi_deg):
    # Convert angles to radians
    theta = np.radians(theta_deg)
    phi = np.radians(phi_deg)

    # Custom system:
    # θ = angle from +X axis toward +Z
    # φ = azimuth from +X toward +Y (around Z axis)
    x = R * np.cos(theta) * np.cos(phi)
    y = R * np.cos(theta) * np.sin(phi)
    z = R * np.sin(theta)
    return x, y, z

# Load the CSV data
csv_file = 'spherical_data.csv'  # Update with your file path if needed

R_list, theta_test, phi_test, theta_real, phi_real = [], [], [], [], []

with open(csv_file, newline='') as f:
    reader = csv.DictReader(f)
    for row in reader:
        R_list.append(float(row["R (cm)"]))
        theta_test.append(float(row["Test θ (°)"]))
        phi_test.append(float(row["Test φ (°)"]))
        theta_real.append(float(row["Real θ (°)"]))
        phi_real.append(float(row["Real φ (°)"]))

# Convert to Cartesian coordinates
test_xyz = [spherical_to_cartesian_custom(R, t, p) for R, t, p in zip(R_list, theta_test, phi_test)]
real_xyz = [spherical_to_cartesian_custom(R, t, p) for R, t, p in zip(R_list, theta_real, phi_real)]

# Unpack for plotting
x_test, y_test, z_test = zip(*test_xyz)
x_real, y_real, z_real = zip(*real_xyz)

# Plotting
fig = plt.figure(figsize=(10, 8))
ax = fig.add_subplot(111, projection='3d')

# Plot test points
ax.scatter(x_test, y_test, z_test, c='blue', label='Test Points', marker='o')

# Plot real points
ax.scatter(x_real, y_real, z_real, c='red', label='Real Points', marker='^')

# Draw lines connecting test and real points
for (xt, yt, zt), (xr, yr, zr) in zip(test_xyz, real_xyz):
    ax.plot([xt, xr], [yt, yr], [zt, zr], color='gray', linestyle='--', linewidth=0.5)

# Labels and legend
ax.set_xlabel('X (cm)')
ax.set_ylabel('Y (cm)')
ax.set_zlabel('Z (cm)')
ax.set_title('3D Comparison of Spherical Points (Test vs Real)')
ax.legend()
plt.tight_layout()
plt.show()
