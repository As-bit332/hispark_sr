import cv2
import numpy as np
import torch
from torch.nn import functional as F
import random


def filter2D(img, kernel):
    """PyTorch version of cv2.filter2D

    Args:
        img (Tensor): (b, c, h, w)
        kernel (Tensor): (b, k, k)
    """
    k = kernel.size(-1)
    b, c, h, w = img.size()
    if k % 2 == 1:
        img = F.pad(img, (k // 2, k // 2, k // 2, k // 2), mode='reflect')
    else:
        raise ValueError('Wrong kernel size')

    ph, pw = img.size()[-2:]

    if kernel.size(0) == 1:
        # apply the same kernel to all batch images
        img = img.view(b * c, 1, ph, pw)
        kernel = kernel.view(1, 1, k, k)
        return F.conv2d(img, kernel, padding=0).view(b, c, h, w)
    else:
        img = img.view(1, b * c, ph, pw)
        kernel = kernel.view(b, 1, k, k).repeat(1, c, 1, 1).view(b * c, 1, k, k)
        return F.conv2d(img, kernel, groups=b * c).view(b, c, h, w)


def usm_sharp(img, weight=0.5, radius=50, threshold=10):
    """USM sharpening.

    Input image: I; Blurry image: B.
    1. sharp = I + weight * (I - B)
    2. Mask = 1 if abs(I - B) > threshold, else: 0
    3. Blur mask:
    4. Out = Mask * sharp + (1 - Mask) * I


    Args:
        img (Numpy array): Input image, HWC, BGR; float32, [0, 1].
        weight (float): Sharp weight. Default: 1.
        radius (float): Kernel size of Gaussian blur. Default: 50.
        threshold (int):
    """
    if radius % 2 == 0:
        radius += 1
    blur = cv2.GaussianBlur(img, (radius, radius), 0)
    residual = img - blur
    mask = np.abs(residual) * 255 > threshold
    mask = mask.astype('float32')
    soft_mask = cv2.GaussianBlur(mask, (radius, radius), 0)

    sharp = img + weight * residual
    sharp = np.clip(sharp, 0, 1)
    return soft_mask * sharp + (1 - soft_mask) * img


class USMSharp(torch.nn.Module):

    def __init__(self, radius=50, sigma=0):
        super(USMSharp, self).__init__()
        if radius % 2 == 0:
            radius += 1
        self.radius = radius
        kernel = cv2.getGaussianKernel(radius, sigma)
        kernel = torch.FloatTensor(np.dot(kernel, kernel.transpose())).unsqueeze_(0)
        self.register_buffer('kernel', kernel)

    def forward(self, img, weight=0.5, threshold=10):
        blur = filter2D(img, self.kernel)
        residual = img - blur

        mask = torch.abs(residual) * 255 > threshold
        mask = mask.float()
        soft_mask = filter2D(mask, self.kernel)
        sharp = img + weight * residual
        sharp = torch.clip(sharp, 0, 1)
        return soft_mask * sharp + (1 - soft_mask) * img


def input_mask(image, prob_=0.75, value=0.1):
    r"""
    Multiplicative bernoulli
    image(numpy)
    """
    embed_dim = image.shape[1]
    x = image.shape[2]
    y = image.shape[3]
    mask = np.random.choice([0, 1], size=(x, y), p=[prob_, 1 - prob_])
    # mask = np.repeat(mask[:, :, np.newaxis], 3, axis=0)
    mask = np.repeat(mask[np.newaxis, :, :], embed_dim, axis=0)
    noise_image = np.multiply(image, mask)
    noise_image = noise_image - value + value * mask
    return noise_image


def input_mask_without_noise(img, sf=1, mask1=75, mask2=75):
    r"""
    Args:
        img(tensor) b, c, h, w
    """
    h1, w1 = img.shape[2:]
    img = img.clone().detach().cpu().numpy()  # 变成numpy数据形式处理
    img = img.copy()[..., :h1 - h1 % sf, :w1 - w1 % sf]  # mod crop

    prob = random.randint(mask1, mask2) / 100
    # prob = 0.75
    img = input_mask(img, prob_=prob)

    img = torch.from_numpy(img).float().cuda()  # dtype float32
    return img
